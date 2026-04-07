#include <Arduino.h>
#include <esp32_smartdisplay.h>
#include "motion/MotionEngine.h"
#include "ui/UI.h"

void setup() {
    smartdisplay_init();
    lv_display_set_rotation(lv_display_get_default(), LV_DISPLAY_ROTATION_270);
    uiInit();
    motionInit();

    smartdisplay_led_set_rgb(0, 255, 0);
    delay(500);
    smartdisplay_led_set_rgb(0, 0, 0);
}

auto lv_last_tick = millis();

void loop() {
    auto const now = millis();
    lv_tick_inc(now - lv_last_tick);
    lv_last_tick = now;
    lv_timer_handler();
    uiUpdate();
    motionLoop();
}
