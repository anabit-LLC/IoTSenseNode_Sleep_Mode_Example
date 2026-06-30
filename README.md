# IoTSenseNode_Sleep_Mode_Example
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
