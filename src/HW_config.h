#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include <Arduino.h>
#include <LED_handler.h>

#define BUILTIN_LED 2
#define dW digitalWrite

void hw_init() {
    pinMode(BUILTIN_LED, OUTPUT); dW(BUILTIN_LED, LOW);
    
    led.init();
}

#endif