#pragma once
#include "config.h"
#include <stdint.h>

enum class MachineState : uint8_t {
    IDLE,
    RUNNING,
    STOP,
    DONE
};

// Written by ISR, read by UI. All fields are volatile.
struct MotionState {
    volatile MachineState state         = MachineState::IDLE;
    volatile bool         stopRequested = false;
    volatile bool         passComplete  = false;
    volatile uint16_t     currentPass   = 0;    // 1-based after first stepover
    volatile uint16_t     totalPasses   = 0;
    volatile int32_t      jobProgress   = 0;
    volatile int32_t      xSteps        = 0;    // current X position in steps
    volatile int32_t      ySteps        = 0;    // current Y position in steps
};

// Written by UI while IDLE, read by motion engine at job start.
// Not volatile — only written when machine is stopped.
struct JobParams {
    float xTravelInches      = X_DEFAULT_TRAVEL;
    float yTravelInches      = Y_DEFAULT_TRAVEL;
    float stepoverInches     = STEPOVER_OPTIONS[STEPOVER_DEFAULT];
    float feedRateIPS        = DEFAULT_FEEDRATE;   // inches per second
    uint16_t dwellMs         = 200;    // dwell time between axis/direction changes
};
