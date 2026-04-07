#pragma once
#include <esp32_smartdisplay.h>

// Build the full LVGL screen. Call once after smartdisplay_init().
void uiInit();

// Poll motion state and update display. Call from loop() every iteration.
void uiUpdate();

// Numpad confirmation callbacks (called by Numpad module).
void onXTravelConfirmed(float inches);
void onYTravelConfirmed(float inches);
