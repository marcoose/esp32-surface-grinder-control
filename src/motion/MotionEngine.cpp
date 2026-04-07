#include "MotionEngine.h"
#include <Arduino.h>
#include <FastAccelStepper.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Global shared state (unchanged)
// ---------------------------------------------------------------------------
MotionState motionState;

// ---------------------------------------------------------------------------
// FastAccelStepper objects
// ---------------------------------------------------------------------------
namespace {

FastAccelStepperEngine engine;
FastAccelStepper* stepperX = nullptr;
FastAccelStepper* stepperY = nullptr;

enum class JobPhase : uint8_t {
    SWEEP_LEFT,
    SWEEP_RIGHT,
    Y_STEPOVER,
    DWELL,
    DONE
};

struct Context {
    JobParams job;
    JobPhase  phase;

    uint16_t passesTotal;
    uint16_t passesDone;

    int32_t xTargetMax;

    int32_t currentXTarget;
    uint32_t dwellStartMs;
    JobPhase lastCompletedPhase;
};

Context ctx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

inline int32_t inchesToSteps(float inches, float stepsPerInch) {
    return (int32_t)(inches * stepsPerInch);
}

inline uint32_t ipsToStepsPerSec(float ips, float stepsPerInch) {
    return (uint32_t)(ips * stepsPerInch);
}

inline uint16_t computeTotalPasses(float yTravelInches, float stepoverInches) {
    return (uint16_t)ceilf(yTravelInches / stepoverInches) + 1;
}

// ---------------------------------------------------------------------------
// Prepare next move parameters (does not change phase)
// ---------------------------------------------------------------------------
void prepareNextMove() {

    switch (ctx.phase) {

        case JobPhase::SWEEP_LEFT: {
            ctx.currentXTarget = ctx.xTargetMax;

            stepperX->setAcceleration(X_ACCEL_STEPS_S2);
            stepperX->setSpeedInHz(
                ipsToStepsPerSec(ctx.job.feedRateIPS, X_STEPS_PER_INCH)
            );

            stepperX->moveTo(ctx.currentXTarget);
            break;
        }

        case JobPhase::SWEEP_RIGHT: {
            ctx.currentXTarget = 0;

            stepperX->setAcceleration(X_ACCEL_STEPS_S2);
            stepperX->setSpeedInHz(
                ipsToStepsPerSec(ctx.job.feedRateIPS, X_STEPS_PER_INCH)
            );

            stepperX->moveTo(ctx.currentXTarget);
            break;
        }

        case JobPhase::Y_STEPOVER: {
            ctx.passesDone++;
            motionState.currentPass  = ctx.passesDone;
            motionState.passComplete = true;

            if (ctx.passesDone >= ctx.passesTotal) {
                ctx.phase = JobPhase::DONE;
                motionState.state = MachineState::DONE;
                motionState.jobProgress = 100;
                return;
            }

            // Y move WITH acceleration now
            int32_t ySteps = inchesToSteps(
                ctx.job.stepoverInches, Y_STEPS_PER_INCH
            );

            stepperY->setAcceleration(Y_ACCEL_STEPS_S2);
            stepperY->setSpeedInHz(
                ipsToStepsPerSec(Y_STEPOVER_IPS, Y_STEPS_PER_INCH)
            );

            stepperY->move(ySteps);
            break;
        }

        case JobPhase::DWELL:
        case JobPhase::DONE:
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Update loop
// ---------------------------------------------------------------------------
void updateMotion() {
    float sweepPercent;

    if (motionState.state == MachineState::IDLE && ctx.phase == JobPhase::DONE) {
        // only when done and UI has reset back to idle
        stepperX->setCurrentPosition(0);
        stepperY->setCurrentPosition(0);
        motionState.stopRequested = false;
        motionState.currentPass   = 0;
        motionState.totalPasses   = 0;
        motionState.jobProgress   = 0;
        motionState.xSteps = 0;
        motionState.ySteps = 0;
        motionState.passComplete  = true;  // this is immediately set to false by UI, but forces pass counter to refresh
    }

    if (motionState.state != MachineState::RUNNING) {
        return;
    }

    // --- Stop handling ---
    if (motionState.stopRequested) {
        stepperX->stopMove();
        stepperY->stopMove();

        if (!stepperX->isRunning() && !stepperY->isRunning()) {
            motionState.state = MachineState::STOP;
        }
        return;
    }

    // --- Handle current phase ---
    switch (ctx.phase) {

        case JobPhase::SWEEP_LEFT:
        case JobPhase::SWEEP_RIGHT: {
            motionState.xSteps = stepperX->getCurrentPosition();
            sweepPercent = (0.5 * motionState.xSteps / ctx.xTargetMax);
            if (ctx.phase == JobPhase::SWEEP_RIGHT) {
                sweepPercent = 1 - sweepPercent;
            }

            motionState.jobProgress =  (motionState.currentPass / (float)motionState.totalPasses + (1.0 / motionState.totalPasses * sweepPercent)) * 100;

            // Wait for X axis to finish moving
            if (!stepperX->isRunning()) {
                // Move complete, start dwell
                ctx.lastCompletedPhase = ctx.phase;
                ctx.phase = JobPhase::DWELL;
                ctx.dwellStartMs = millis();
            }
            break;
        }

        case JobPhase::Y_STEPOVER: {
            motionState.ySteps = stepperY->getCurrentPosition();
            // Wait for Y axis to finish moving
            if (!stepperY->isRunning()) {
                // Move complete, update position and start dwell
                ctx.lastCompletedPhase = ctx.phase;
                ctx.phase = JobPhase::DWELL;
                ctx.dwellStartMs = millis();
            }
            break;
        }

        case JobPhase::DWELL: {
            // Wait for dwell time
            if (millis() - ctx.dwellStartMs >= ctx.job.dwellMs) {
                // Dwell complete, transition to next phase
                switch (ctx.lastCompletedPhase) {
                    case JobPhase::SWEEP_LEFT:
                        ctx.phase = JobPhase::SWEEP_RIGHT;
                        break;
                    case JobPhase::SWEEP_RIGHT:
                        ctx.phase = JobPhase::Y_STEPOVER;
                        break;
                    case JobPhase::Y_STEPOVER:
                        ctx.phase = JobPhase::SWEEP_LEFT;
                        break;
                    default:
                        break;
                }
                // Prepare the next move
                prepareNextMove();
            }
            break;
        }

        default:
            break;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void motionInit() {

    engine.init();

    stepperX = engine.stepperConnectToPin(PIN_X_STEP);
    stepperY = engine.stepperConnectToPin(PIN_Y_STEP);

    if (stepperX) {
        stepperX->setDirectionPin(PIN_X_DIR);
    }

    if (stepperY) {
        stepperY->setDirectionPin(PIN_Y_DIR);
    }

    motionState.state = MachineState::IDLE;
}

// ---------------------------------------------------------------------------

void motionStart(const JobParams& params) {

    motionState.state         = MachineState::RUNNING;
    motionState.stopRequested = false;
    motionState.passComplete  = false;
    motionState.currentPass   = 0;
    motionState.totalPasses   = computeTotalPasses(params.yTravelInches, params.stepoverInches);
    motionState.jobProgress   = 0;

    motionState.xSteps = 0;
    motionState.ySteps = 0;

    ctx.job         = params;
    ctx.passesTotal = motionState.totalPasses;
    ctx.passesDone  = 0;
    ctx.phase       = JobPhase::SWEEP_LEFT;
    ctx.lastCompletedPhase = JobPhase::DONE;  // Initialize to something invalid

    ctx.xTargetMax = inchesToSteps(params.xTravelInches, X_STEPS_PER_INCH);

    stepperX->setCurrentPosition(0);

    // Kick off first move (SWEEP_LEFT: move to max)
    prepareNextMove();
}

// ---------------------------------------------------------------------------
// Runtime feed rate updates
// ---------------------------------------------------------------------------
void motionSetFeedRate(float feedRateIPS) {
    ctx.job.feedRateIPS = feedRateIPS;
    if (motionState.state == MachineState::RUNNING &&
        (ctx.phase == JobPhase::SWEEP_LEFT || ctx.phase == JobPhase::SWEEP_RIGHT) &&
        stepperX->isRunning()) {
        stepperX->setSpeedInHz(
            ipsToStepsPerSec(feedRateIPS, X_STEPS_PER_INCH)
        );
        stepperX->applySpeedAcceleration();
    }
}

// ---------------------------------------------------------------------------

void motionLoop() {
    updateMotion();
}

void IRAM_ATTR requestStop() {
    motionState.stopRequested = true;
}