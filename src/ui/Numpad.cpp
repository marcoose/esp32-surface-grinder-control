#include "Numpad.h"
#include <lvgl.h>
#include <string.h>
#include <stdio.h>

static lv_obj_t*       overlay    = nullptr;
static lv_obj_t*       textarea   = nullptr;
static lv_group_t*     input_group = nullptr;
static float           maxAllowed = 0;
static NumpadConfirmCb confirmCb  = nullptr;

/* -------------------- Helpers -------------------- */

static bool append_char(const char* txt) {
    if (!textarea || !txt) return false;

    const char* current = lv_textarea_get_text(textarea);

    // Only allow one decimal point
    if (txt[0] == '.' && strchr(current, '.')) return false;

    // Length limit (adjust as needed)
    if (strlen(current) >= 8) return false;

    lv_textarea_add_text(textarea, txt);
    return true;
}

bool validateNumpadInput(const char* str, float maxVal, float* out) {
    if (str == nullptr || str[0] == '\0') return false;
    if (strcmp(str, ".") == 0) return false;

    char* end = nullptr;
    float val = strtof(str, &end);

    if (*end != '\0') return false;
    if (val <= 0.0f)  return false;
    if (val > maxVal)  return false;

    *out = val;
    return true;
}

/* -------------------- Event Handlers -------------------- */

static void on_btnmatrix_event(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t* obj = lv_event_get_target_obj(e);
    uint32_t id   = lv_btnmatrix_get_selected_btn(obj);

    if (id == LV_BTNMATRIX_BTN_NONE) return;

    const char* txt = lv_btnmatrix_get_btn_text(obj, id);
    if (!txt) return;

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_delete_char(textarea);
    } else {
        append_char(txt);
    }

    lv_group_focus_obj(textarea);
}

static void on_confirm_pressed(lv_event_t* e) {
    const char* valStr = lv_textarea_get_text(textarea);

    float result = 0;
    if (validateNumpadInput(valStr, maxAllowed, &result)) {
        NumpadConfirmCb cb = confirmCb;
        numpadClose();
        if (cb) cb(result);
    } else {
        // Optional: don’t clear, just highlight error
        lv_obj_set_style_border_color(textarea, lv_color_hex(0xf85149), 0);
    }
}

static void on_close_pressed(lv_event_t* e) {
    numpadClose();
}

/* -------------------- Public API -------------------- */

void numpadOpen(const char* title, float currentVal, float maxVal, NumpadConfirmCb onConfirm) {
    if (overlay) numpadClose();

    maxAllowed = maxVal;
    confirmCb  = onConfirm;

    char initial[16];
    snprintf(initial, sizeof(initial), "%.3f", currentVal);

    /* ---------- Overlay ---------- */
    overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(overlay, 480, 320);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* ---------- Header ---------- */
    lv_obj_t* hdr = lv_obj_create(overlay);
    lv_obj_set_size(hdr, 480, 32);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* titleLbl = lv_label_create(hdr);
    lv_label_set_text(titleLbl, title);
    lv_obj_align(titleLbl, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(titleLbl, lv_color_hex(0x8b949e), 0);

    lv_obj_t* closeBtn = lv_btn_create(hdr);
    lv_obj_set_size(closeBtn, 32, 32);
    lv_obj_align(closeBtn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(closeBtn, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(closeBtn, on_close_pressed, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* closeLbl = lv_label_create(closeBtn);
    lv_label_set_text(closeLbl, LV_SYMBOL_CLOSE);
    lv_obj_center(closeLbl);
    lv_obj_set_style_text_color(closeLbl, lv_color_hex(0x8b949e), 0);

    /* ---------- Text Area ---------- */
    textarea = lv_textarea_create(overlay);
    lv_obj_set_size(textarea, 200, 46);
    lv_obj_set_pos(textarea, 8, 39);
    lv_textarea_set_text(textarea, initial);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_cursor_click_pos(textarea, true);

    lv_obj_set_style_bg_color(textarea, lv_color_hex(0x161b22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(textarea, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_border_color(textarea, lv_color_hex(0x30363d), LV_PART_MAIN);
    lv_obj_set_style_border_width(textarea, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(textarea, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(textarea, 4, LV_PART_MAIN); // was 8

    lv_obj_set_style_text_color(textarea, lv_color_hex(0x3fb950), LV_PART_MAIN);
    lv_obj_set_style_text_font(textarea, &lv_font_montserrat_28, LV_PART_MAIN);

    /* Cursor (important for visibility on dark bg) */
    lv_obj_set_style_border_side(textarea, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(textarea, lv_color_white(), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(textarea, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(textarea, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);

    // Input group for text area focus restoration after button press
    input_group = lv_group_create();
    lv_group_add_obj(input_group, textarea);
    lv_group_focus_obj(textarea);

    char hint[32];
    snprintf(hint, sizeof(hint), "inches (max %.0f\")", maxVal);

    lv_obj_t* hintLbl = lv_label_create(overlay);
    lv_label_set_text(hintLbl, hint);
    lv_obj_set_pos(hintLbl, 320, 55);
    lv_obj_set_style_text_color(hintLbl, lv_color_hex(0x686f78), 0);

    /* ---------- Button Matrix ---------- */
    static const char* btn_map[] = {
        "7", "8", "9", "\n",
        "4", "5", "6", "\n",
        "1", "2", "3", "\n",
        ".", "0", LV_SYMBOL_BACKSPACE, ""
    };

    lv_obj_t* btnm = lv_btnmatrix_create(overlay);
    lv_obj_set_size(btnm, 464, 170);
    lv_obj_set_pos(btnm, 8, 90);
    lv_btnmatrix_set_map(btnm, btn_map);
    lv_obj_add_event_cb(btnm, on_btnmatrix_event, LV_EVENT_VALUE_CHANGED, nullptr);

    /* Matrix background */
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0x0d1117), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnm, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnm, 0, LV_PART_MAIN);

    /* Buttons */
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0x30363d), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);

    lv_obj_set_style_border_color(btnm, lv_color_hex(0x30363d), LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 4, LV_PART_ITEMS);

    lv_obj_set_style_text_color(btnm, lv_color_hex(0xe6edf3), LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, &lv_font_montserrat_28, LV_PART_ITEMS);

    lv_obj_set_style_bg_color(btnm, lv_color_hex(0x30363d), LV_PART_ITEMS | LV_STATE_PRESSED);

    // Tint backspace red
    // Backspace is button index 11 in your map
    lv_btnmatrix_set_btn_ctrl(btnm, 11, LV_BTNMATRIX_CTRL_CUSTOM_1);
    lv_obj_set_style_text_color(btnm, lv_color_hex(0x8d1f1f), LV_PART_ITEMS | LV_BTNMATRIX_CTRL_CUSTOM_1);


    /* --- Confirmation button --- */
    lv_obj_t* confirmBtn = lv_btn_create(overlay);
    lv_obj_set_size(confirmBtn, 464, 46);
    lv_obj_set_pos(confirmBtn, 8, 320 - 46); // bottom-aligned
    lv_obj_set_style_bg_color(confirmBtn, lv_color_hex(0x238636), 0);
    lv_obj_set_style_radius(confirmBtn, 6, 0);

    lv_obj_add_event_cb(confirmBtn, on_confirm_pressed, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* confLbl = lv_label_create(confirmBtn);
    lv_label_set_text(confLbl, "CONFIRM");
    lv_obj_center(confLbl);
    lv_obj_set_style_text_color(confLbl, lv_color_hex(0xe6edf3), 0);
    lv_obj_set_style_text_font(confLbl, &lv_font_montserrat_28, 0);

    lv_group_focus_obj(textarea);
}

/* -------------------- Close -------------------- */

void numpadClose() {
    if (overlay) {
        lv_obj_del(overlay);
        overlay   = nullptr;
        textarea  = nullptr;
        confirmCb = nullptr;
    }

    if (input_group) {
        lv_group_del(input_group);
        input_group = nullptr;
    }
}