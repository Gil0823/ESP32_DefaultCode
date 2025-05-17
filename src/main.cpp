#include <Arduino.h>
#include <SimpleTimer.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include <LittleFS.h>
#define BUILTIN_LED 2
#define FOR(i, b, e) for(int i = b; i < e; i++)
#define dW digitalWrite

enum {
    CONNECTED=-2
};

// 1. 네트워크 연결 되면 5초마다 2번 빠르게 점멸
// 2. WiFi 정보는 LIFFS에 저장
// 3. 주변에 있는 WiFi와 LIFFS 저장되있는 WiFi와 일치 시 자동 연결 시도
// 4. 주변에 와이파이가 없을 경우 LED 빠르게 점멸
// 5. 저장되있는 비밀번호로 5초 이상 연결 시도에도 무반응 시 연결 차단
// 6. 연결 상태에서 갑작스러운 연결 해제 시 감지 가능
// 7. 연결 해제 시 2번의 스캔 Term을 가진 후 연결 재시도

// 수집한 와이파이 정보 객체
typedef struct Wifi_info {
    String ssid;
    String RSSI;
    String Encryption;  // 암호화 여부
} Wifi_info;

String ssid_list[8];
String password_list[8];
int block_idx[8] = {false, };
int list_size = 0;
int match_idx = -1;  // 연결 성공한 WiFi 인덱스 ( 0~7 )
unsigned long begin_timestamp = 0;

String mqtt_server = "<브로커서버주소>";

WiFiClientSecure espClient;
PubSubClient client(espClient);

SimpleTimer printTimer(1000);
SimpleTimer ledTimer(99999);
SimpleTimer ledTimer_fast(99999);
SimpleTimer reconnectTimer(2000);
SimpleTimer scanTimer(2500);
SimpleTimer connect_timeout_Timer(5000);
SimpleTimer connectTimer(100);
bool isScaning = false;
bool isConnecting = false;
bool isNoAvailable_wifi = false;
bool isDisconnectedEvent = false;
bool ledStat = false;
int ledBlink_cnt = -1;
int isCur_ledCycle_end = false;

void WiFiEvent(WiFiEvent_t event);
bool begin_network_setup();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
int find_available_network();
void onWiFiDisconnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
bool isAuthFailureLike(uint8_t reason);

void setup() {
    Serial.begin(115200); // 시리얼 통신 초기화

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &onWiFiDisconnect, NULL); // 이벤트 핸들러 등록
    
    pinMode(BUILTIN_LED, OUTPUT);

    if(!LittleFS.begin(true)) Serial.println("An Error has occurred while mounting LittleFS");

    File file = LittleFS.open("/wifi.txt");
    if(!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.println("File Content:");
    if(file.available()) {
        String content = file.readString();

        int split_idx = 0;
        int prev_idx = 0;
        int count = 0;
        for (int i = 0; i < content.length(); i++) {
            if (content.charAt(i) == '\n') count++;
        }
        while(list_size <= count) {
            int split_idx = content.indexOf(" ");
            ssid_list[list_size] = content.substring(0, split_idx).c_str();

            prev_idx = split_idx;

            split_idx = content.indexOf("\n", split_idx+1);
            password_list[list_size] = content.substring(prev_idx+1, split_idx-1).c_str();

            content.remove(0, split_idx+1);

            list_size++;
        }
        
        FOR(i, 0, list_size) Serial.printf("[%d] SSID(%s), PWD(%s)\n", i, ssid_list[i].c_str(), password_list[i].c_str());
    }

    file.close();
    
    dW(BUILTIN_LED, LOW);
    
    WiFi.scanNetworks(true);   
    isScaning = true;
    
    connect_timeout_Timer.reset();
    ledTimer.reset();
    ledTimer_fast.reset();
    reconnectTimer.reset();
    scanTimer.reset(); 
}

void loop() {
    static int wifi_cnt = -1;
    static unsigned long discon_time;
    static unsigned long ledTime;
    
    wifi_cnt = WiFi.scanComplete();  
    
    // Network 설정 시작 후 연결 됐는지 확인
    if(isConnecting && connectTimer.isReady()) {
        Serial.print(".");
        
        if (WiFi.isConnected()) {
            randomSeed(micros());
            
            Serial.println("");
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());

            // 연결이 성공적이므로 연결대기 상태를 CONNECTED로 변경
            block_idx[match_idx] = CONNECTED;

            // MQTT연결 설정
            espClient.setInsecure();
            client.setServer(mqtt_server.c_str(), 8883);
            client.setCallback(callback);
            reconnect();
            
            ledTimer.setInterval(5000); ledBlink_cnt = 0;
            ledTimer_fast.setInterval(100);
            dW(BUILTIN_LED, LOW);
            
            isConnecting = false;
            isScaning = false;
            
            isDisconnectedEvent = false;
        }
        
        connectTimer.reset();
    }

    // WiFI 연결 해제 감지
    if (!WiFi.isConnected() || isDisconnectedEvent) { 
        // 이 코드가 실행될 때까지 맥락: SPIFFS에 저장되있는 와이파이가 내 주변에 있고, 나는 거기에 연결 시도를 함 ( 내 DB를 바탕으로 )
        // 연결 중 일때 Timeout 이라면 맥락에 따라 -> 무조건 비번 틀림상태 확정 가능
        if (isConnecting && connect_timeout_Timer.isReady()) {
            Serial.printf("\n[169줄] %s(%d) - 타임아웃.\n", ssid_list[match_idx].c_str(), match_idx);

            // 완전한 중단
            WiFi.disconnect(true, true);
            WiFi.mode(WIFI_OFF);

            // 가장 중요한 과정으로, 현재 비밀번호가 틀린 WiFi정보를 비활성화
            if (match_idx != -1) {
                Serial.printf("[AUTH-FAIL] %s → 영구 차단\n", ssid_list[match_idx].c_str());
                block_idx[match_idx] = -1;
            }

            isScaning = false;    // 스캔 시작하도록 스캔상태를 false로 지정
            isConnecting = false;
        
            connect_timeout_Timer.reset();
        } 
        // AP종료를 여기서 잡아냄
        // 현재 block_idx[match_idx]가 0이면 방금전까지 연결되있던 거임
        // 현재 스캔이 시작될 때 까지 해당 조건문 실행을 멈추도록 함 ( 폭주 현상 발생 )
        else if (scanTimer.isReady() && !isScaning && block_idx[match_idx] == CONNECTED) {
            Serial.printf("[AP-OFF] %s → AP전원 꺼짐\n", ssid_list[match_idx].c_str());

            // 스캔 2번 이후에 연결하도록 설정
            block_idx[match_idx] = 2;

            ledTimer_fast.setInterval(50);
            ledTimer.setInterval(2000);

            // 완전한 중단
            WiFi.disconnect(true, true);
            WiFi.mode(WIFI_OFF);
        }

        isDisconnectedEvent = false;  // 다음 이벤트 트리거 장전
    }
    
    // SPIFFS에 저장된 WiFi없을 시 주변에 사용가능한 와이파이 계속 탐색
    if (!WiFi.isConnected() && !isConnecting && !isScaning && scanTimer.isReady()) {
        Serial.println("[208줄] 스캔시작.");

        // 현재 스캔정보 초기화
        WiFi.scanDelete();  
        wifi_cnt = -1;
        
        WiFi.scanNetworks(true);
        isScaning = true;
        
        // begin_timestamp = millis();
        
        ledTimer.setInterval(500); ledBlink_cnt = -1;  // fast 안씀
        dW(BUILTIN_LED, LOW); ledStat = HIGH;
        
        scanTimer.reset();
    }
    
    // 비동기 스캔이 완료 됐을 때
    if(!isConnecting && isScaning && 0 <= wifi_cnt) {
        Serial.println("[227줄] 스캔완료.");
        
        char tmp[128]; memset(tmp, '\0', 64);

        Wifi_info scaned_list[wifi_cnt];

        // 스캔된 와이파이 정보 전처리
        FOR(i, 0, wifi_cnt) {
            scaned_list[i].ssid = WiFi.SSID(i);
            scaned_list[i].RSSI = String(WiFi.RSSI(i));
            scaned_list[i].Encryption = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "" : "[*]";
        }

        String msg = "";
        FOR(i, 0, wifi_cnt) {
            memset(tmp, '\0', 64);

            // 출력할 문자열을 한 줄 씩 생성후 msg에 연결(concat)
            sprintf(
                tmp, 
                "SSID: %s%s (%sdbm)\n", 
                scaned_list[i].ssid.c_str(), scaned_list[i].Encryption.c_str(), scaned_list[i].RSSI.c_str()
            );
            
            msg += tmp;
        }

        // 전처리한 정보 출력
        Serial.println(msg);
        
        // 만약 현재 네트워크에 연결되어 있지 않을 시 스캔 완료한 정보를 토대로 Network 구성 시작
        if (!WiFi.isConnected()) {
            // begin_timestamp = millis();
            begin_network_setup();
        } else {
            // MQTT에 연결돼있을 시 정보 전송
            Serial.printf("Send data size: %dbyte", msg.length());
            if (client.connected()) {
                client.beginPublish("status", msg.length(), false);
                client.print(msg.c_str());
                client.endPublish();
            }
            
            // 스캔데이터 초기화
            WiFi.scanDelete();
            wifi_cnt = -1;
            
        }
        isScaning = false;  

        scanTimer.reset();
    }
    
    // LED 담당
    if(ledTimer.isReady()) {
        // fast를 안쓴다면
        if (ledBlink_cnt < 0) {
            dW(BUILTIN_LED, ledStat);
            ledStat = !ledStat;        
        } 
        // fast를 쓴다면
        else {
            if (!isConnecting && WiFi.isConnected()) {
            ledBlink_cnt = 4;  // 2번 점멸
            ledStat = HIGH;
            } 
            // SPIFFS에 저장된 와이파이가 없을 때
            else if (!isConnecting && !WiFi.isConnected()) {
                ledBlink_cnt = 10;  // 5번 점멸
                ledStat = HIGH;
            } 
            
        }
        
        ledTimer.reset();
    }
    
    // 빠르게 점멸하는 기능
    if(0 < ledBlink_cnt && ledTimer_fast.isReady()) {
        digitalWrite(BUILTIN_LED, ledStat);
        
        ledStat = !ledStat;
        
        ledBlink_cnt--;

        ledTimer_fast.reset();
    }
    
    // MQTT접속에 실패 시
    if (!client.connected() && reconnectTimer.isReady()) {
        if (!isConnecting && WiFi.isConnected()) reconnect(); // WiFi연결 상태에서만 재연결
        
        reconnectTimer.reset();
    }

    client.loop();
}

//  SPIFFS에 저장되있는 WiFi가 주변에 있는지 검색( return: ssid_list의 인덱스 )
int search_available_network(int wifi_cnt) {
    String find_ssid, find_pwd;
    int find_idx = -1;

    FOR(i, 0, wifi_cnt) {      
        FOR(j, 0, list_size) {
            // SPIFFS에 저장된 WiFi가 있으며 비밀번호가 틀린 이력이 없는가
            if (block_idx[j] != -1 && ssid_list[j] == String(WiFi.SSID(i))) {
                return j;
            }
        }
    }

    return find_idx;
}

// Network 구성 ( 전제조건으로 WiFi 스캔이 완료되있어야 함 )
bool begin_network_setup() {
    // SPIFFS에 저장되있는 WiFi가 주변에 있는지 검색
    int find_idx = search_available_network(WiFi.scanComplete());

    // 이전에 연결 실패한 SSID 대기시간 갱신
    if (match_idx != -1 && 0 < block_idx[match_idx]) {
        // 다음 연결 시도 까지 남은 대기횟수
        Serial.printf("[AUTH-FAIL] %s → %d회 대기\n", ssid_list[match_idx].c_str(), block_idx[match_idx]);

        block_idx[match_idx]--;
        
        return false;
    }

    // 검색 결과 없으면 주기적으로 연결 재시도 시작
    if (find_idx == -1) {
        Serial.println("!!!! 사용가능한 와이파이 없음 !!!!");
        
        isConnecting = false;
        isScaning = false;
        
        ledTimer.setInterval(2000); ledBlink_cnt = 0;
        ledTimer_fast.setInterval(50);  // 빠르게 점멸 시작 - 사용자에게 핫스팟을 키던 어떻게던 조치하도록 강조
        dW(BUILTIN_LED, LOW);
        
        // 스캔데이터 초기화
        WiFi.scanDelete();

        return false;
    }
    
    match_idx = find_idx;  // 현재 매칭된 인덱스 저장

    Serial.println("Connecting to " + ssid_list[find_idx]);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_list[find_idx].c_str(), password_list[find_idx].c_str());
    
    // 여기까지 왔으면 SPIFFS에 저장된 정보로 연결 시도를 할꺼임
    isConnecting = true;
    
    ledTimer.setInterval(200); ledBlink_cnt = -1;  // fast 안씀
    dW(BUILTIN_LED, LOW); ledStat = HIGH;
    
    // Timeout 타이머 초기화
    connect_timeout_Timer.reset();
    
    return true;
}

const char* getDisconnectReasonString(uint8_t reason) {
    switch (reason) {
        case 1:  return "UNSPECIFIED";
        case 2:  return "AUTH_EXPIRE";
        case 3:  return "AUTH_LEAVE";
        case 4:  return "ASSOC_EXPIRE";
        case 5:  return "ASSOC_TOOMANY";
        case 6:  return "NOT_AUTHED";
        case 7:  return "NOT_ASSOCED";
        case 8:  return "ASSOC_LEAVE";
        case 9:  return "ASSOC_NOT_AUTHED";
        case 10: return "DISASSOC_PWRCAP_BAD";
        case 11: return "DISASSOC_SUPCHAN_BAD";
        case 12: return "IE_INVALID";
        case 13: return "MIC_FAILURE";
        case 14: return "4WAY_HANDSHAKE_TIMEOUT";
        case 15: return "AUTH_FAIL";
        case 16: return "CIPHER_SUITE_REJECTED";
        case 17: return "BEACON_TIMEOUT";
        case 18: return "NO_AP_FOUND";
        case 201: return "NO_AP_FOUND (soft)";
        case 202: return "AUTH_FAIL (soft)";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        default: return "UNKNOWN_REASON";
    }
}

void onWiFiDisconnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != WIFI_EVENT && event_id != WIFI_EVENT_STA_DISCONNECTED) return;

    wifi_event_sta_disconnected_t* discon = (wifi_event_sta_disconnected_t*) event_data;

    Serial.printf("Disconnected. Reason: %s\n", getDisconnectReasonString(discon->reason)); 
    isDisconnectedEvent = true;

    switch(discon->reason) {
        case WIFI_REASON_AUTH_EXPIRE:
            Serial.printf("[WIFI_REASON_AUTH_EXPIRE] %dms\n", millis()-begin_timestamp);
            break;
    }
}

void reconnect() {
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), "<브로커 접속 ID>", "<접속 ID의 비밀번호>")) {
        Serial.println("connected!!");

        // 접속이 완료되면 본인의 내부아이피 주소 전송
        client.publish("status", ("wake-up! : " + WiFi.localIP().toString()).c_str());
        client.subscribe("cmd");
    } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 2 seconds");
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String recv;

    for (int i = 0; i < length; i++) recv += (char)payload[i];
    
    Serial.printf("Message arrived [%s] > %s\n", topic, recv.c_str());

    if (recv == "SPIFFS" || recv == "spi") {
        char msg[256]; memset(msg, '\0', 256);

        sprintf(msg, "Total: %dbyte\n Used: %dbyte", LittleFS.totalBytes(), LittleFS.usedBytes());
        
        client.publish("status", msg);

        return;
    }
    if (recv == "Network" || recv == "net") {
        char tmp[128]; memset(tmp, '\0', 128);

        sprintf(tmp, "[현재 연결된 와이파이]\nSSID: %s (%sdbm)\n내부아이피: %s\n", 
            WiFi.SSID().c_str(), 
            String(WiFi.RSSI()).c_str(), 
            WiFi.localIP().toString().c_str()
        );

        client.publish("status", tmp);
        
        // 비동기 스캔 시작
        int wifi_cnt = WiFi.scanNetworks(true);
        isScaning = true;

        return;
    }
}