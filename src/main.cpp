#include <Arduino.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <env.h>
#include <HW_config.h>
#include <Network_config.h>
#include <LED_handler.h>
#include <esp_camera.h>
#include "WifiCam.hpp"
#define FOR(i, b, e) for(int i = b; i < e; i++)



esp32cam::Resolution initialResolution;
WebServer server(80);

// 1. 네트워크 연결 되면 5초마다 2번 빠르게 점멸
// 2. WiFi 정보는 LIFFS에 저장
// 3. 주변에 있는 WiFi와 LIFFS 저장되있는 WiFi와 일치 시 자동 연결 시도
// 4. 주변에 와이파이가 없을 경우 LED 빠르게 점멸
// 5. 저장되있는 비밀번호로 5초 이상 연결 시도에도 무반응 시 연결 차단
// 6. 연결 상태에서 갑작스러운 연결 해제 시 감지 가능
// 7. LittleFS저장소를 FTP를 통해 접근 및 수정 가능

bool checkPhoto( fs::FS &fs ) {//function to check the photo in spiffs memory
    File f_pic = fs.open( "/photo.jpg" );
    unsigned int pic_sz = f_pic.size();
    Serial.printf("size = %d\n", pic_sz);
    return ( pic_sz > 100 );
}

void capturePhotoSave() { // capture a photo and save to spiffs
    camera_fb_t * fb = NULL; // pointer
    
    bool ok = 0; // Boolean indicating if the picture has been taken correctly

    do {
        // Take a photo with the camera
        Serial.println("Taking a photo...");

        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            return;
        }

        // Photo file name
        Serial.printf("Picture file name: %s\n", "/photo.jpg");
        File file = LittleFS.open("/photo.jpg", FILE_WRITE);

        // Insert the data in the photo file
        if (!file) {
            Serial.println("Failed to open file in writing mode");
        }
        else {
            file.write(fb->buf, fb->len); // payload (image), payload length
            Serial.printf("fb->len: %d\n", fb->len);
            
            Serial.print("The picture has been saved in ");
            Serial.print("/photo.jpg");
            Serial.print(" - Size: ");
            Serial.print(file.size());
            Serial.println(" bytes");
        }
        // Close the file
        file.close();
        esp_camera_fb_return(fb);

        // check if file has been correctly saved in LittleFS
        ok = checkPhoto(LittleFS);
    } while ( !ok );
}

void setup() {
    Serial.begin(115200); // 시리얼 통신 초기화

    hw_init();
    env.init();
    net.init();
    
    {
        using namespace esp32cam;
        
        Config cfg;
        cfg.setCustom();

        cfg.setPins(pins::AiThinker);
        // cfg.setResolution(initialResolution);
        

        bool ok = Camera.begin(cfg);
        if (!ok) {
            Serial.println("camera initialize failure");
            delay(5000);
            ESP.restart();
        }
        
        Camera.setFlip();
        Serial.println("camera initialize success");
    }
}

bool streamBeginCheck = false;

void loop() {   
    if (!streamBeginCheck) {
        if (net.isConnected()) {
            Serial.printf("camera starting: http://%s\n", WiFi.localIP().toString().c_str());
            
            addRequestHandlers();
            server.begin();
            
            streamBeginCheck = true;
        }
    }
    if (net.isAvailable()) {
        String recv = net.get_mqtt_recv();
        
        // 사용중인 저장소 용량 확인하는 명령어
        if (recv == "LittleFS" || recv == "lfs") {
            char msg[256]; memset(msg, '\0', 256);

            sprintf(msg, "Total: %dbyte\nUsed: %dbyte", LittleFS.totalBytes(), LittleFS.usedBytes());
            
            mqtt_client.publish("status", msg);

            return;
        }
        // 현재 접속된 WiFi 및 주변 WiFi 확인하는 명령어
        if (recv == "Network" || recv == "net") {
            char tmp[128]; memset(tmp, '\0', 128);

            sprintf(tmp, "[현재 연결된 와이파이]\nSSID: %s (%ddbm)\n내부아이피: %s\n", 
                WiFi.SSID().c_str(), 
                WiFi.RSSI(), 
                WiFi.localIP().toString().c_str()
            );

            mqtt_client.publish("status", tmp);
            
            // 비동기 스캔 시작
            WiFi.scanNetworks(true);

            return;
        }
        if (recv == "shot") {
            capturePhotoSave();
        }
    }
    
    if (net.isConnected()) {
        server.handleClient();
    }

    
    net.run();
    led.run();
}