#ifndef CAM_STREAMING_HANDLER_H
#define CAM_STREAMING_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HW_config.h>
#include <SimpleTimer.h>
#include <Network_config.h>
#include "rtsp/OV2640Streamer.h"
#include "rtsp/CRtspSession.h"
#include <esp_camera.h>
#include <base64.h>
#include <WiFiUdp.h>

#define RTSP 0
#define UDP  1
#define CHUNK_LENGTH 1460

// TODO: UDP 스트리밍 추가
class Cam_Stream_Handler {
    private:
        SimpleTimer  rtsp_frameTimer;  // RTSP 프레임 타이머
        CStreamer    *streamer;
        CRtspSession *session;
        WiFiClient    rtspClient;
        WiFiServer    rtspServer;
        uint8_t msecPerFrame;
        int rtspPort;

        SimpleTimer  udp_frameTimer;  // RTSP 프레임 타이머
        WiFiUDP udp;
        int udpPort;
        
        String encoded;

        int cur_mode;

    public:
        Cam_Stream_Handler() = default;
        Cam_Stream_Handler& operator=(const Cam_Stream_Handler& ref) = delete;  
        static Cam_Stream_Handler& GetInstance();
        
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
                    encoded = base64::encode(fb->buf, fb->len);
                    
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
        
        bool sendLargeBase64MQTT() {
            // 크기가 크기 때문에 꼭 call by ref가 되도록 주소값을 전달
            net.publish("status", &encoded);

            Serial.println("Base64 image sent via MQTT");
            return true;
        }

        void end() {
            rtspServer.end();
        }

        void init() {
            cur_mode = RTSP;

            rtsp_frameTimer.setInterval(100);  // 10fps
            msecPerFrame = 100;
            rtspPort = 8554;

            char tmp[64]; memset(tmp, '\0', 64);

            sprintf(tmp, "RTSP 스트리밍 시작: rtsp://%s:%d/mjpeg/1", 
                    WiFi.localIP().toString().c_str(), 
                    rtspPort
            );

            net.publish("status", tmp);

            session  = NULL;
            streamer = NULL;

            rtspServer.begin(rtspPort);  // rtsp서버 포트 8554 설정

            udp_frameTimer.setInterval(50);  // 20fps ( UDP라 빨라도 됨 )

            udpPort = 8000; // UDP서버 포트 8000 설정
            // udp.begin(WiFi.localIP(), udpPort);
        }

        void sendPacketData(const char* buf, uint16_t len, uint16_t chunkLength) {
            uint8_t buffer[chunkLength];
            size_t blen = sizeof(buffer);
            size_t rest = len % blen;

            for (uint8_t i = 0; i < len / blen; ++i) {
                memcpy(buffer, buf + (i * blen), blen);
                udp.beginPacket("192.168.0.5", 8000);
                udp.write(buffer, chunkLength);
                udp.endPacket();
            }

            if (rest) {
                memcpy(buffer, buf + (len - rest), rest);
                udp.beginPacket("192.168.0.5", 8000);
                udp.write(buffer, rest);
                udp.endPacket();
            }
        }

        void setFrameRate(int rate, int mode=RTSP) {
            switch(mode) {
                case RTSP:
                    if (rate < 50) rate = 50;  // 20fps 이상으로는 못가게 함

                    rtsp_frameTimer.setInterval(rate);  // rate msec만큼 갱신
                    msecPerFrame = rate;

                    break;
                case UDP:
                    if (rate < 33) rate = 33;  // 30fps 이상으로는 못가게 함

                    udp_frameTimer.setInterval(rate);  // rate msec만큼 갱신

                    break;
            }
        }

        void set_mode(int _mode) {
            if (cur_mode == _mode) return;
            
            switch(cur_mode) {
                case RTSP:
                    rtspServer.close();

                    break;
                case UDP:
                    udp.stop();

                    break;
            }

            switch(_mode) {
                case RTSP:
                    rtspServer.begin(rtspPort);
                    
                    break;
                case UDP:
                    udp.begin(WiFi.localIP(), udpPort);

                    break;

            }

            cur_mode = _mode;

        }

        void run() {
            static unsigned long lastimage = millis();
            static unsigned long now       = millis();

            if (!WiFi.isConnected()) return;

            if (cur_mode == RTSP) {
                if (session) {
                    session->handleRequests(0);
            
                    if (rtsp_frameTimer.isReady()) {
                        lastimage = millis();

                        session->broadcastCurrentFrame(now);
                        
                        if(millis() - lastimage >= msecPerFrame)
                            printf("warning exceeding max frame rate of %d ms\n", millis() - lastimage);

                        rtsp_frameTimer.reset();
                    }
            
                    if(session->m_stopped) {
                        delete session;
                        delete streamer;
                        session = NULL;
                        streamer = NULL;
                    }
                }
                else {
                    rtspClient = rtspServer.accept();
            
                    if(rtspClient) {
                        streamer = new OV2640Streamer(&rtspClient, cam);
                        session  = new CRtspSession(&rtspClient, streamer); 
                    }
                }

                return;
            }

            if (cur_mode == UDP) {
                if (!udp_frameTimer.isReady()) return;

                camera_fb_t* fb = NULL;
                esp_err_t res   = ESP_OK;

                fb = esp_camera_fb_get();
                
                if (!fb) {
                    Serial.println("Camera capture failed");
                    esp_camera_fb_return(fb);
                    return;
                }

                if (fb->format != PIXFORMAT_JPEG) {
                    Serial.println("PIXFORMAT_JPEG not implemented");
                    esp_camera_fb_return(fb);
                    return;
                }

                sendPacketData((const char*)fb->buf, fb->len, CHUNK_LENGTH);
                esp_camera_fb_return(fb);

                udp_frameTimer.reset();
            }
        }

        int getPort() {
            return rtspPort;
        }




}; Cam_Stream_Handler& cam_streamer = Cam_Stream_Handler::GetInstance();

Cam_Stream_Handler& Cam_Stream_Handler::GetInstance() {
    static Cam_Stream_Handler instance;
    
    return instance;
}





#endif