#include "UI.h"
#include "Numpad.h"
#include "../motion/MotionEngine.h"
#include "../config.h"
#include <lvgl.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout constants (pixels)
// ---------------------------------------------------------------------------
static const int16_t SCREEN_W = 480;
static const int16_t SCREEN_H = 320;
static const int16_t STATUS_H = 28;
static const int16_t PAD      = 4;
static const int16_t COL_W    = (SCREEN_W - PAD * 4) / 3;  // ~156
static const int16_t COL0     = PAD;
static const int16_t COL1     = PAD + COL_W + PAD;
static const int16_t COL2     = PAD + (COL_W + PAD) * 2;
static const int16_t ROW0     = STATUS_H * 2 + PAD * 3;   // top cards
static const int16_t CARD_H   = 110;
static const int16_t ROW1     = ROW0 + CARD_H + PAD;      // feed rate
static const int16_t FEED_H   = 70;
static const int16_t ROW2     = ROW1 + FEED_H + PAD;      // buttons
static const int16_t STEP_BTN_H = 32;

// ---------------------------------------------------------------------------
// Color palette
// ---------------------------------------------------------------------------
static const uint32_t C_BG       = 0x0d1117;
static const uint32_t C_CARD     = 0x161b22;
static const uint32_t C_BORDER   = 0x30363d;
static const uint32_t C_GREEN    = 0x3fb950;
static const uint32_t C_AMBER    = 0xd29922;
static const uint32_t C_RED      = 0xf85149;
static const uint32_t C_DIM      = 0x8b949e;
static const uint32_t C_TEXT     = 0xe6edf3;
static const uint32_t C_BTN_GO   = 0x238636;
static const uint32_t C_BTN_STOP = 0xab1a1a;

// ---------------------------------------------------------------------------
// Widget handles
// ---------------------------------------------------------------------------
static lv_obj_t* statusDot;
static lv_obj_t* statusLabel;
static lv_obj_t* xPosLabel;
static lv_obj_t* yPosLabel;

static lv_obj_t* xCard;
static lv_obj_t* xValueLabel;
static lv_obj_t* yCard;
static lv_obj_t* yValueLabel;

static lv_obj_t* stepoverBtns[STEPOVER_COUNT];

static lv_obj_t* feedSlider;
static lv_obj_t* feedValueLabel;

static lv_obj_t* startBtn;
static lv_obj_t* stopBtn;
static lv_obj_t* resetBtn;

static lv_obj_t* passLabel;
static lv_obj_t* progressBar;

// ---------------------------------------------------------------------------
// Job parameters — written by UI while IDLE
// ---------------------------------------------------------------------------
static JobParams jobParams;
static int selectedStepover = STEPOVER_DEFAULT;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static lv_obj_t* makeCard(lv_obj_t* parent, int16_t x, int16_t y, int16_t w, int16_t h) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(C_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 8, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    return card;
}

static void setWidgetEnabled(lv_obj_t* obj, bool enabled) {
    if (enabled) lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else         lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void setInputsEnabled(bool enabled) {
    setWidgetEnabled(xCard, enabled);
    setWidgetEnabled(yCard, enabled);
    // setWidgetEnabled(feedSlider, enabled);  leave feedrate enabled always for live override
    setWidgetEnabled(startBtn, enabled);
    setWidgetEnabled(resetBtn, enabled);
    for (int i = 0; i < STEPOVER_COUNT; i++) {
        setWidgetEnabled(stepoverBtns[i], enabled);
    }
    // STOP is the inverse
    setWidgetEnabled(stopBtn, !enabled);
}

float sliderToFeedRate(int sliderVal, float minIPS, float maxIPS) {
    if (sliderVal <= 0)   return minIPS;
    if (sliderVal >= 100) return maxIPS;
    return minIPS + ((float)sliderVal / 100.0f) * (maxIPS - minIPS);
}

// ---------------------------------------------------------------------------
// Event callbacks
// ---------------------------------------------------------------------------
static void onXCardClicked(lv_event_t* e) {
    numpadOpen("ENTER X TRAVEL", jobParams.xTravelInches, X_MAX_INCHES, onXTravelConfirmed);
}

static void onYCardClicked(lv_event_t* e) {
    numpadOpen("ENTER Y TRAVEL", jobParams.yTravelInches, Y_MAX_INCHES, onYTravelConfirmed);
}

static void onStepoverClicked(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    selectedStepover = idx;
    jobParams.stepoverInches = STEPOVER_OPTIONS[idx];
    for (int i = 0; i < STEPOVER_COUNT; i++) {
        if (i == idx) lv_obj_add_state(stepoverBtns[i], LV_STATE_CHECKED);
        else          lv_obj_remove_state(stepoverBtns[i], LV_STATE_CHECKED);
    }
}

static void onFeedChanged(lv_event_t* e) {
    int32_t val = lv_slider_get_value(feedSlider);
    jobParams.feedRateIPS = sliderToFeedRate(val, FEED_MIN_IPS, FEED_MAX_IPS);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f in/s", jobParams.feedRateIPS);
    lv_label_set_text(feedValueLabel, buf);

    if (motionState.state == MachineState::RUNNING) {
        motionSetFeedRate(jobParams.feedRateIPS);
    }
}

static void onStartClicked(lv_event_t* e) {
    if (motionState.state != MachineState::IDLE) return;
    setInputsEnabled(false);
    motionStart(jobParams);
}

static void onStopClicked(lv_event_t* e) {
    requestStop();
}

static void onResetClicked(lv_event_t* e) {
    motionState.state = MachineState::IDLE;
}

// ---------------------------------------------------------------------------
// uiInit
// ---------------------------------------------------------------------------
void uiInit() {
    static lv_style_t style_prgbar_indic;

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    // === Status bar ===
    lv_obj_t* bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, STATUS_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x010409), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);

    statusDot = lv_obj_create(bar);
    lv_obj_set_size(statusDot, 8, 8);
    lv_obj_set_pos(statusDot, 8, 10);
    lv_obj_set_style_radius(statusDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(statusDot, lv_color_hex(C_GREEN), LV_PART_MAIN);
    lv_obj_set_style_border_width(statusDot, 0, LV_PART_MAIN);

    statusLabel = lv_label_create(bar);
    lv_label_set_text(statusLabel, "IDLE");
    lv_obj_set_pos(statusLabel, 22, 7);
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(C_GREEN), LV_PART_MAIN);

    xPosLabel = lv_label_create(bar);
    lv_label_set_text(xPosLabel, "X: 0.000\"");
    lv_obj_set_pos(xPosLabel, 310, 7);
    lv_obj_set_style_text_color(xPosLabel, lv_color_hex(C_TEXT), LV_PART_MAIN);

    yPosLabel = lv_label_create(bar);
    lv_label_set_text(yPosLabel, "Y: 0.000\"");
    lv_obj_set_pos(yPosLabel, 408, 7);
    lv_obj_set_style_text_color(yPosLabel, lv_color_hex(C_TEXT), LV_PART_MAIN);

    // === Progress panel ===
    lv_obj_t* progPanel = makeCard(scr, COL0, STATUS_H + PAD, SCREEN_W - 8, STATUS_H + PAD);

    passLabel = lv_label_create(progPanel);
    lv_label_set_text(passLabel, "Pass -/--");
    lv_obj_set_pos(passLabel, 0, 0);
    lv_obj_set_style_text_color(passLabel, lv_color_hex(C_AMBER), LV_PART_MAIN);

    lv_style_init(&style_prgbar_indic);
    lv_style_set_border_color(&style_prgbar_indic, lv_palette_main(LV_PALETTE_BLUE_GREY));
    lv_style_set_border_width(&style_prgbar_indic, 1);
    lv_style_set_anim_duration(&style_prgbar_indic, 200);

    progressBar = lv_bar_create(progPanel);
    lv_obj_set_size(progressBar, SCREEN_W - 120, 16);
    lv_obj_set_pos(progressBar, 90, 0);
    lv_bar_set_range(progressBar, 0, 100);
    lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progressBar, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar, lv_color_hex(C_GREEN), LV_PART_INDICATOR);
    lv_obj_add_style(progressBar, &style_prgbar_indic, 0);

    // === X Travel card ===
    xCard = makeCard(scr, COL0, ROW0, COL_W, CARD_H);
    lv_obj_add_flag(xCard, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(xCard, onXCardClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(xCard);
    lv_label_set_text(lbl, "X TRAVEL");
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_DIM), LV_PART_MAIN);
    lv_obj_set_pos(lbl, 0, 0);

    xValueLabel = lv_label_create(xCard);
    lv_obj_set_pos(xValueLabel, 0, 40);
    lv_obj_set_style_text_color(xValueLabel, lv_color_hex(C_GREEN), LV_PART_MAIN);
    lv_obj_set_style_text_font(xValueLabel, &lv_font_montserrat_28, LV_PART_MAIN);
    onXTravelConfirmed(jobParams.xTravelInches);

    lbl = lv_label_create(xCard);
    lv_label_set_text(lbl, "tap to edit");
    lv_obj_set_pos(lbl, 0, 102);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x484f58), LV_PART_MAIN);

    // === Y Travel card ===
    yCard = makeCard(scr, COL1, ROW0, COL_W, CARD_H);
    lv_obj_add_flag(yCard, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(yCard, onYCardClicked, LV_EVENT_CLICKED, nullptr);

    lbl = lv_label_create(yCard);
    lv_label_set_text(lbl, "Y TRAVEL");
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_DIM), LV_PART_MAIN);
    lv_obj_set_pos(lbl, 0, 0);

    yValueLabel = lv_label_create(yCard);
    lv_obj_set_pos(yValueLabel, 0, 40);
    lv_obj_set_style_text_color(yValueLabel, lv_color_hex(C_GREEN), LV_PART_MAIN);
    lv_obj_set_style_text_font(yValueLabel, &lv_font_montserrat_28, LV_PART_MAIN);
    onYTravelConfirmed(jobParams.yTravelInches);

    lbl = lv_label_create(yCard);
    lv_label_set_text(lbl, "tap to edit");
    lv_obj_set_pos(lbl, 0, 102);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x484f58), LV_PART_MAIN);

    // === Stepover panel ===  (spans travel and feed rows)
    lv_obj_t* stepPanel = makeCard(scr, COL2, ROW0, COL_W, CARD_H + FEED_H + PAD);

    lbl = lv_label_create(stepPanel);
    lv_label_set_text(lbl, "STEPOVER");
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_DIM), LV_PART_MAIN);
    lv_obj_set_pos(lbl, 0, 0);

    const char* stepLabels[] = { ".020\"", ".050\"", ".100\"", ".200\"" };
    for (int i = 0; i < STEPOVER_COUNT; i++) {
        stepoverBtns[i] = lv_btn_create(stepPanel);
        lv_obj_set_size(stepoverBtns[i], COL_W - 16, STEP_BTN_H);
        lv_obj_set_pos(stepoverBtns[i], 0, 22 + i * (STEP_BTN_H + 4));
        lv_obj_set_style_bg_color(stepoverBtns[i], lv_color_hex(C_BORDER), LV_PART_MAIN);
        lv_obj_set_style_bg_color(stepoverBtns[i], lv_color_hex(C_GREEN), LV_STATE_CHECKED);
        lv_obj_set_style_radius(stepoverBtns[i], 4, LV_PART_MAIN);
        lv_obj_add_flag(stepoverBtns[i], LV_OBJ_FLAG_CHECKABLE);
        if (i == selectedStepover) lv_obj_add_state(stepoverBtns[i], LV_STATE_CHECKED);
        lv_obj_add_event_cb(stepoverBtns[i], onStepoverClicked, LV_EVENT_CLICKED,
                            (void*)(intptr_t)i);

        lv_obj_t* btnLbl = lv_label_create(stepoverBtns[i]);
        lv_label_set_text(btnLbl, stepLabels[i]);
        lv_obj_center(btnLbl);
        lv_obj_set_style_text_color(btnLbl, lv_color_hex(C_TEXT), LV_PART_MAIN);
    }

    // === Feed rate row (spans 2 columns) ===
    lv_obj_t* feedPanel = makeCard(scr, COL0, ROW1, COL_W * 2 + PAD, FEED_H);

    lbl = lv_label_create(feedPanel);
    lv_label_set_text(lbl, "FEED RATE");
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_DIM), LV_PART_MAIN);
    lv_obj_set_pos(lbl, 0, 0);

    feedSlider = lv_slider_create(feedPanel);
    lv_obj_set_size(feedSlider, COL_W * 2 - 60, 16);
    lv_obj_set_pos(feedSlider, 18, 25);
    lv_slider_set_range(feedSlider, 0, 100);
    lv_slider_set_value(feedSlider, ((DEFAULT_FEEDRATE - FEED_MIN_IPS) / (FEED_MAX_IPS - FEED_MIN_IPS) * 100), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(feedSlider, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(feedSlider, lv_color_hex(C_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(feedSlider, lv_color_hex(C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(feedSlider, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(feedSlider, onFeedChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    feedValueLabel = lv_label_create(feedPanel);
    lv_obj_set_pos(feedValueLabel, COL_W * 2 + PAD - 100, 0);
    lv_obj_set_style_text_color(feedValueLabel, lv_color_hex(C_AMBER), LV_PART_MAIN);
    onFeedChanged(nullptr);

    // === Control buttons ===
    int16_t btnW = (SCREEN_W - PAD * 4) / 3;
    int16_t btnH = SCREEN_H - ROW2 - PAD;

    startBtn = lv_btn_create(scr);
    lv_obj_set_size(startBtn, btnW, btnH);
    lv_obj_set_pos(startBtn, COL0, ROW2);
    lv_obj_set_style_bg_color(startBtn, lv_color_hex(C_BTN_GO), LV_PART_MAIN);
    lv_obj_set_style_radius(startBtn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(startBtn, onStartClicked, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(startBtn);
    lv_label_set_text(lbl, LV_SYMBOL_PLAY " START");
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_TEXT), LV_PART_MAIN);

    resetBtn = lv_btn_create(scr);
    lv_obj_set_size(resetBtn, btnW, btnH);
    lv_obj_set_pos(resetBtn, COL1, ROW2);
    lv_obj_set_style_bg_color(resetBtn, lv_color_hex(C_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_color(resetBtn, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(resetBtn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(resetBtn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(resetBtn, onResetClicked, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(resetBtn);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH " RESET");
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_DIM), LV_PART_MAIN);

    stopBtn = lv_btn_create(scr);
    lv_obj_set_size(stopBtn, btnW, btnH);
    lv_obj_set_pos(stopBtn, COL2, ROW2);
    lv_obj_set_style_bg_color(stopBtn, lv_color_hex(C_BTN_STOP), LV_PART_MAIN);
    lv_obj_set_style_radius(stopBtn, 6, LV_PART_MAIN);
    lv_obj_add_state(stopBtn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(stopBtn, onStopClicked, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(stopBtn);
    lv_label_set_text(lbl, LV_SYMBOL_STOP " STOP");
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);

    // Initial state
    setInputsEnabled(true);
}

// ---------------------------------------------------------------------------
// uiUpdate — called every loop() iteration
// ---------------------------------------------------------------------------
static MachineState prevState = MachineState::IDLE;
static uint32_t nextUpdate = millis() + 50;

void uiUpdate() {
    if (millis() < nextUpdate) {
        return;
    }
    nextUpdate = millis() + 50;

    MachineState cur = motionState.state;

    // Detect state transitions
    if (cur != prevState) {
        if (cur == MachineState::IDLE) {
            lv_label_set_text(statusLabel, "IDLE");
            lv_obj_set_style_text_color(statusLabel, lv_color_hex(C_GREEN), LV_PART_MAIN);
            lv_obj_set_style_bg_color(statusDot, lv_color_hex(C_GREEN), LV_PART_MAIN);
            smartdisplay_led_set_rgb(0, 0, 0);
            setInputsEnabled(true);
        } else if (cur == MachineState::RUNNING) {
            lv_label_set_text(statusLabel, "RUNNING");
            lv_obj_set_style_text_color(statusLabel, lv_color_hex(C_AMBER), LV_PART_MAIN);
            lv_obj_set_style_bg_color(statusDot, lv_color_hex(C_AMBER), LV_PART_MAIN);
            smartdisplay_led_set_rgb(220, 230, 0);
            motionState.passComplete = true; // fake a complete pass when starting to update the progress
            setInputsEnabled(false);
        } else if (cur == MachineState::DONE) {
            lv_label_set_text(statusLabel, "DONE");
            lv_obj_set_style_text_color(statusLabel, lv_color_hex(C_GREEN), LV_PART_MAIN);
            lv_obj_set_style_bg_color(statusDot, lv_color_hex(C_GREEN), LV_PART_MAIN);
            smartdisplay_led_set_rgb(0, 255, 0);
            setInputsEnabled(true);
        }  else if (cur == MachineState::STOP) {
            lv_label_set_text(statusLabel, "E-STOP");
            lv_obj_set_style_text_color(statusLabel, lv_color_hex(C_RED), LV_PART_MAIN);
            lv_obj_set_style_bg_color(statusDot, lv_color_hex(C_RED), LV_PART_MAIN);
            smartdisplay_led_set_rgb(255, 0, 0);
            setInputsEnabled(true);
        }
        prevState = cur;
    }

    // Update position readout
    char buf[20];
    snprintf(buf, sizeof(buf), "X: %.3f\"", (float)motionState.xSteps / X_STEPS_PER_INCH);
    lv_label_set_text(xPosLabel, buf);
    snprintf(buf, sizeof(buf), "Y: %.3f\"", (float)motionState.ySteps / Y_STEPS_PER_INCH);
    lv_label_set_text(yPosLabel, buf);

    lv_bar_set_value(progressBar, motionState.jobProgress, LV_ANIM_OFF);

    // Update pass counter when motion signals a completed pass
    if (motionState.passComplete) {
        motionState.passComplete = false;
        int curPass = motionState.currentPass + 1;  // motionState.currentPass = current latest completed pass, not 'in progress' pass
        if (curPass > motionState.totalPasses) {
            curPass = motionState.totalPasses;
        }
        snprintf(buf, sizeof(buf), "Pass %u/%u", curPass, motionState.totalPasses);
        lv_label_set_text(passLabel, buf);
    }

}

// ---------------------------------------------------------------------------
// Numpad callbacks
// ---------------------------------------------------------------------------
void onXTravelConfirmed(float inches) {
    jobParams.xTravelInches = inches;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.3f\"", inches);
    lv_label_set_text(xValueLabel, buf);
}

void onYTravelConfirmed(float inches) {
    jobParams.yTravelInches = inches;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.3f\"", inches);
    lv_label_set_text(yValueLabel, buf);
}
