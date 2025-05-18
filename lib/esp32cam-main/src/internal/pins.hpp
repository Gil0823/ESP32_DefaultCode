#ifndef ESP32CAM_PINS_HPP
#define ESP32CAM_PINS_HPP

namespace esp32cam {

/** @brief Camera pins definition. */
struct Pins {
  int D0;
  int D1;
  int D2;
  int D3;
  int D4;
  int D5;
  int D6;
  int D7;
  int XCLK;
  int PCLK;
  int VSYNC;
  int HREF;
  int SDA;
  int SCL;
  int RESET;
  int PWDN;
};

namespace pins {

/** @brief Pin definition for AI Thinker ESP32-CAM. */
constexpr Pins AiThinker{
  D0: 5,   
  D1: 18,
  D2: 19,
  D3: 21,
  D4: 36,
  D5: 39,
  D6: 34,
  D7: 35,
  XCLK: 0,
  PCLK: 22,
  VSYNC: 25,
  HREF: 23,
  SDA: 26,
  SCL: 27,
  RESET: -1,
  PWDN: 32,
  /*
  #define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5

#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13
  */
};

/** @brief Pin definition for FREENOVE WROVER ESP32-CAM. */
constexpr Pins FreeNove{
  D0: 4,
  D1: 5,
  D2: 18,
  D3: 19,
  D4: 36,
  D5: 39,
  D6: 34,
  D7: 35,
  XCLK: 21,
  PCLK: 22,
  VSYNC: 25,
  HREF: 23,
  SDA: 26,
  SCL: 27,
  RESET: -1,
  PWDN: -1,
};

/** @brief Pin definition for M5Stack M5Camera. */
constexpr Pins N16R8 {
  D0: 11,   
  D1: 9,
  D2: 8,
  D3: 10,
  D4: 12,
  D5: 18,
  D6: 17,
  D7: 16,
  XCLK: 15,
  PCLK: 13,
  VSYNC: 6,
  HREF: 7,
  SDA: 4,
  SCL: 5,
  RESET: -1,
  PWDN: -1,
};

/** @brief Pin definition for M5Stack M5Camera. */
constexpr Pins M5Camera{
  D0: 32,
  D1: 35,
  D2: 34,
  D3: 5,
  D4: 39,
  D5: 18,
  D6: 36,
  D7: 19,
  XCLK: 27,
  PCLK: 21,
  VSYNC: 25,
  HREF: 26,
  SDA: 22,
  SCL: 23,
  RESET: 15,
  PWDN: -1,
};

/**
 * @brief Pin definition for M5Stack M5Camera with LED.
 *
 * Red LED on GPIO 14, tally light when tied to PWDN
 */
constexpr Pins M5CameraLED{
  D0: 32,
  D1: 35,
  D2: 34,
  D3: 5,
  D4: 39,
  D5: 18,
  D6: 36,
  D7: 19,
  XCLK: 27,
  PCLK: 21,
  VSYNC: 25,
  HREF: 26,
  SDA: 22,
  SCL: 23,
  RESET: 15,
  PWDN: 14,
};

/** @brief Pin definition for TTGO ESP32-CAM. */
constexpr Pins TTGO{
  D0: 5,
  D1: 14,
  D2: 4,
  D3: 15,
  D4: 37,
  D5: 38,
  D6: 36,
  D7: 39,
  XCLK: 32,
  PCLK: 19,
  VSYNC: 27,
  HREF: 25,
  SDA: 13,
  SCL: 12,
  RESET: -1,
  PWDN: -1,
};

} // namespace pins
} // namespace esp32cam

#endif // ESP32CAM_PINS_HPP
