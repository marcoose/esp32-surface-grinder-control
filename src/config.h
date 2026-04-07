#pragma once

// ---------------------------------------------------------------------------
// GPIO assignments
// ---------------------------------------------------------------------------
#define PIN_X_STEP      21
#define PIN_X_DIR       22
#define PIN_Y_STEP      18
#define PIN_Y_DIR       19
#define PIN_ESTOP       35  // input-only, reserved for future use

// ---------------------------------------------------------------------------
// Axis calibration
// ---------------------------------------------------------------------------
#define X_STEPS_PER_INCH    3200.0f

#define Y_STEPS_PER_INCH    1600.0f

// ---------------------------------------------------------------------------
// Feed rates
// ---------------------------------------------------------------------------
#define FEED_MIN_IPS        0.25f   // inches per second
#define FEED_MAX_IPS        4.0f
#define Y_STEPOVER_IPS      0.1f

// ---------------------------------------------------------------------------
// Acceleration — tune empirically during commissioning
// ---------------------------------------------------------------------------
#define X_ACCEL_STEPS_S2    1500.0f   // steps/sec^2
#define Y_ACCEL_STEPS_S2    1500.0f   // steps/sec^2

// ---------------------------------------------------------------------------
// Travel limits (numpad validation bounds)
// ---------------------------------------------------------------------------
#define X_MAX_INCHES        12.25f
#define Y_MAX_INCHES        6.25f

// ---------------------------------------------------------------------------
// Travel defaults (distance and speed)
// ---------------------------------------------------------------------------
#define X_DEFAULT_TRAVEL  3.0f
#define Y_DEFAULT_TRAVEL  0.5f
#define DEFAULT_FEEDRATE  3.0f

// ---------------------------------------------------------------------------
// Stepover options
// ---------------------------------------------------------------------------
static const float STEPOVER_OPTIONS[] = { 0.020f, 0.050f, 0.100f, 0.200f };
static const int   STEPOVER_COUNT     = 4;
static const int   STEPOVER_DEFAULT   = 2;  // index into STEPOVER_OPTIONS → .100"
