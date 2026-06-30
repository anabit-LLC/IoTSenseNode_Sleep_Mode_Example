/********************************************************************************************************
This Arduino sketch demonstrates timed light sleep and deep sleep on Anabit's IoT SenseNode with ESP32-S3.

Product link:

The board wakes for the configured time, flashes the RGB LED through several colors, then sleeps for the
same amount of time. In deep sleep mode the ESP32-S3 restarts after each wake, so the wake demo runs from
setup() before the board goes back to sleep.

In the Arduino programming environment use the board manager profile "ESP32S3 Dev Module" to compile and 
load this sketch.

Required library: FastLED

Example code developed by Your Anabit LLC (c) 2026
Licensed under the Apache License, Version 2.0.
********************************************************************************************************/

#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

// Sleep mode options. Set ANABIT_SLEEP_MODE to one of these values.
#define ANABIT_LIGHT_SLEEP 0
#define ANABIT_DEEP_SLEEP 1

// User setting: choose ANABIT_LIGHT_SLEEP or ANABIT_DEEP_SLEEP.
// Light sleep resumes after the timer wakeup. Deep sleep restarts the sketch after the timer wakeup.
#ifndef ANABIT_SLEEP_MODE
#define ANABIT_SLEEP_MODE ANABIT_LIGHT_SLEEP
#endif

// User setting: awake time and sleep time, in milliseconds.
// Default is 3000 ms awake, then 3000 ms asleep.
#ifndef ANABIT_SLEEP_TIME_MS
#define ANABIT_SLEEP_TIME_MS 3000UL
#endif

// RGB LED settings for the IoT SenseNode onboard WS2812.
#define NUM_LEDS 1
#define DATA_PIN 1
#define LED_BRIGHTNESS 50
#define LED_FLASH_TIME_MS 250UL

CRGB leds[NUM_LEDS];

// Colors shown while the board is awake.
const CRGB wakeColors[] = {
  CRGB::Red,
  CRGB::Green,
  CRGB::Blue,
  CRGB::Yellow,
  CRGB::Purple,
  CRGB::Cyan
};

const size_t wakeColorCount = sizeof(wakeColors) / sizeof(wakeColors[0]);

// Function prototypes. Helper method bodies are kept after loop().
void setLed(const CRGB& color);
void turnLedOff();
void releaseHeldPinsAfterDeepSleep();
void disableRadios();
void preparePinsForSleep();
void flashLedDuringWakeWindow(uint32_t wakeTime_ms);
void enterTimedSleep(uint32_t sleepTime_ms);
void runWakeThenSleepCycle();

/*
  Initializes serial output, disables unused radios, configures the RGB LED,
  prints the selected sleep mode, and starts the first wake/sleep cycle.
  Arguments: none.
  Returns: nothing.
*/
void setup() {
  Serial.begin(115200);
  delay(100);

  // If the previous cycle used deep sleep, release the held LED data pin before using FastLED.
  releaseHeldPinsAfterDeepSleep();

  // Keep radio current out of the sleep-mode measurement.
  disableRadios();

  // The SenseNode RGB LED uses GRB color order.
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  turnLedOff();

#if ANABIT_SLEEP_MODE == ANABIT_DEEP_SLEEP
  Serial.println("IoT SenseNode ESP32-S3 deep sleep demo.");
#else
  Serial.println("IoT SenseNode ESP32-S3 light sleep demo.");
#endif

  // Run one wake/sleep cycle immediately after boot.
  // In deep sleep mode, the sketch restarts here on every timer wakeup.
  runWakeThenSleepCycle();
}

/*
  Runs repeated wake/sleep cycles when light sleep mode is selected. Deep sleep
  never resumes in loop() because the ESP32-S3 restarts after each timer wakeup.
  Arguments: none.
  Returns: nothing.
*/
void loop() {
#if ANABIT_SLEEP_MODE == ANABIT_LIGHT_SLEEP
  // Light sleep resumes here after waking, so loop starts the next cycle.
  runWakeThenSleepCycle();
#endif

  // Deep sleep never resumes here because the ESP32-S3 restarts after wakeup.
}

/*
  Sets the onboard RGB LED to a requested color and immediately updates the LED.
  Arguments:
    color - FastLED CRGB color value to display.
  Returns: nothing.
*/
void setLed(const CRGB& color) {
  leds[0] = color;
  FastLED.show();
}

/*
  Turns the onboard RGB LED off.
  Arguments: none.
  Returns: nothing.
*/
void turnLedOff() {
  setLed(CRGB::Black);
}

/*
  Releases any GPIO hold left over from a previous deep sleep cycle so FastLED
  can control the RGB LED data pin after the ESP32-S3 wakes and restarts.
  Arguments: none.
  Returns: nothing.
*/
void releaseHeldPinsAfterDeepSleep() {
  // Deep sleep can hold GPIO states. Release the hold before configuring the LED again.
  gpio_hold_dis((gpio_num_t)DATA_PIN);
  gpio_deep_sleep_hold_dis();
}

/*
  Turns off WiFi and Bluetooth so radio current is not included in the sleep
  mode measurement.
  Arguments: none.
  Returns: nothing.
*/
void disableRadios() {
  // WiFi and Bluetooth are not used in this demo, so turn them off before measuring current.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
}

/*
  Clears the RGB LED and drives the LED data pin low before sleep so the pin is
  not floating and the LED is not left on.
  Arguments: none.
  Returns: nothing.
*/
void preparePinsForSleep() {
  // Make sure the addressable LED is off before the MCU sleeps.
  FastLED.clear(true);
  FastLED.show();
  delay(20);

  // Drive the LED data pin low so it is not left floating during sleep.
  pinMode(DATA_PIN, OUTPUT);
  digitalWrite(DATA_PIN, LOW);
}

/*
  Flashes the RGB LED through the wake color list for the requested awake time.
  Arguments:
    wakeTime_ms - awake window length in milliseconds.
  Returns: nothing.
*/
void flashLedDuringWakeWindow(uint32_t wakeTime_ms) {
  uint32_t start_ms = millis();
  size_t colorIndex = 0;

  // Keep flashing colors until the configured awake window has elapsed.
  while ((millis() - start_ms) < wakeTime_ms) {
    setLed(wakeColors[colorIndex]);
    colorIndex = (colorIndex + 1) % wakeColorCount;
    delay(LED_FLASH_TIME_MS);
  }

  turnLedOff();
}

/*
  Configures the ESP32-S3 timer wakeup source and enters the selected sleep mode.
  In deep sleep mode this function does not return because the chip restarts on
  wake. In light sleep mode this function returns after the timer wakeup.
  Arguments:
    sleepTime_ms - sleep duration in milliseconds.
  Returns: nothing.
*/
void enterTimedSleep(uint32_t sleepTime_ms) {
  uint64_t sleepTime_us = (uint64_t)sleepTime_ms * 1000ULL;

  Serial.print("Entering ");

#if ANABIT_SLEEP_MODE == ANABIT_DEEP_SLEEP
  Serial.print("deep");
#else
  Serial.print("light");
#endif

  Serial.print(" sleep for ");
  Serial.print(sleepTime_ms);
  Serial.println(" ms.");
  Serial.flush();

  disableRadios();
  preparePinsForSleep();

  // Configure the ESP32-S3 timer as the wake source.
  esp_sleep_enable_timer_wakeup(sleepTime_us);

#if ANABIT_SLEEP_MODE == ANABIT_DEEP_SLEEP
  // Hold the LED data pin low while the chip is in deep sleep.
  gpio_hold_en((gpio_num_t)DATA_PIN);
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
#else
  esp_light_sleep_start();
  Serial.println("Woke from light sleep.");
#endif
}

/*
  Runs one full demo cycle: print the awake time, flash the LED while awake,
  then enter the configured sleep mode for the same amount of time.
  Arguments: none.
  Returns: nothing.
*/
void runWakeThenSleepCycle() {
  Serial.print("Awake for ");
  Serial.print(ANABIT_SLEEP_TIME_MS);
  Serial.println(" ms.");

  flashLedDuringWakeWindow(ANABIT_SLEEP_TIME_MS);
  enterTimedSleep(ANABIT_SLEEP_TIME_MS);
}
