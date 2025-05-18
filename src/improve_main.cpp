// #include "WifiCam.hpp"
// #include <WiFi.h>
// #include <Arduino.h>

// static const char* WIFI_SSID = "iptime_lig";
// static const char* WIFI_PASS = "my$weethome";

// esp32cam::Resolution initialResolution;

// WebServer server(80);

// void
// setup() {
//   Serial.begin(115200);
//   Serial.println();
//   delay(1000);

//   if(psramFound()) {
//     Serial.println("있음!");
//   }

//   WiFi.persistent(false);
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(WIFI_SSID, WIFI_PASS);
//   if (WiFi.waitForConnectResult() != WL_CONNECTED) {
//     Serial.printf("WiFi failure %d\n", WiFi.status());
//     delay(5000);
//     ESP.restart();
//   }
//   Serial.println("WiFi connected");
//   delay(1000);

//   {
//     using namespace esp32cam;
    
//     // initialResolution = Resolution::find(1024, 768);

//     Config cfg;
//     cfg.setCustom();

//     cfg.setPins(pins::AiThinker);
//     // cfg.setResolution(initialResolution);
    
    

//     bool ok = Camera.begin(cfg);
//     if (!ok) {
//       Serial.println("camera initialize failure");
//       delay(5000);
//       ESP.restart();
//     }
//     Camera.setFlip();
//     Serial.println("camera initialize success");
//   }

  

//   Serial.println("camera starting");
//   Serial.print("http://");
//   Serial.println(WiFi.localIP());

//   addRequestHandlers();
//   server.begin();
// }

// void
// loop() {
//   server.handleClient();
// }
