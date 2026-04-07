#pragma once
#include <Arduino.h>
#include "MotionTypes.h"
#include "config.h"

// Global motion state — readable by UI, writable by ISR.
extern MotionState motionState;

// Call once in setup(). Configures GPIO pins and arms the hardware timer.
void motionInit();

// Call in main loop().
void motionLoop();

// Begin a grinding job. Only call when motionState.state == IDLE.
void motionStart(const JobParams& params);

// Dynamically update X feedrate while a job is running.
// This updates the cached job settings and applies new speed to X during sweeps.
void motionSetFeedRate(float feedRateIPS);

// Request graceful stop. Decelerates to halt, then IDLE.
// Safe to call from main loop or from an ISR (e.g. future GPIO35 e-stop).
void IRAM_ATTR requestStop();
