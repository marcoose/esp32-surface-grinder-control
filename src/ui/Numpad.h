#pragma once

// Callback type — receives the validated float value.
typedef void (*NumpadConfirmCb)(float value);

// Open a full-screen numpad overlay.
//   title      — header text (e.g. "ENTER X TRAVEL")
//   currentVal — pre-filled value shown in the display
//   maxVal     — validation ceiling (> 0 and <= maxVal required)
//   onConfirm  — called with the confirmed value; modal auto-closes
void numpadOpen(const char* title, float currentVal, float maxVal, NumpadConfirmCb onConfirm);

// Close the numpad without confirming. Called internally on X button or after confirm.
void numpadClose();
