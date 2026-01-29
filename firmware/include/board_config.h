#pragma once
// =====================================================
// Board Pin Configuration
// =====================================================
// Central location for all hardware pin definitions.
// Override any pin via build flags in platformio.ini:
//   build_flags = -DSTATUS_LED0_PIN=PA5
//
// For STM32CubeIDE migration, define these in main.h
// using HAL GPIO_PIN_x constants.
// =====================================================

#include <stdint.h>

// For Arduino builds, include Arduino.h to get pin definitions (PA5, PA6, etc.)
#ifdef ARDUINO
#include <Arduino.h>
#endif

// =====================================================
// Status LED Pins
// =====================================================
// LED 0: Device readiness/handshake status
// LED 1: Master/Slave role indication

#ifndef STATUS_LED0_PIN
#ifdef PA5
#define STATUS_LED0_PIN PA5   // Arduino: PA5 (D13)
#else
#define STATUS_LED0_PIN 5     // Generic: GPIO 5
#endif
#endif

#ifndef STATUS_LED1_PIN
#ifdef PA6
#define STATUS_LED1_PIN PA6   // Arduino: PA6 (D12)
#else
#define STATUS_LED1_PIN 6     // Generic: GPIO 6
#endif
#endif

// =====================================================
// Tap Link Pin
// =====================================================
// 1-wire open-drain interface for device-to-device communication

#ifndef TAP_LINK_PIN
#ifdef PA9
#define TAP_LINK_PIN PA9      // Arduino: PA9 (D8)
#else
#define TAP_LINK_PIN 9        // Generic: GPIO 9
#endif
#endif

// =====================================================
// Buzzer Pin (HS-F02A or similar passive piezo)
// =====================================================
// PWM-capable pin for tone generation
// NUCLEO-L053R8: D9 = PC7 (TIM3_CH2 or TIM22_CH2)

#ifndef BUZZER_PIN
#ifdef PC7
#define BUZZER_PIN PC7        // Arduino: PC7 (D9) - PWM capable
#else
#define BUZZER_PIN 9          // Generic: GPIO 9
#endif
#endif