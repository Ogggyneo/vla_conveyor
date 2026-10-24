#include "stepper_motor.h"

namespace {
// Slow PC817 optocouplers on TB6600 clones miss short pulses at 3.3 V.
constexpr uint32_t STEP_PULSE_US = 20;
constexpr uint32_t DIR_SETUP_US = 10;     // TB6600 needs >= 5 us DIR setup.
// ENA+ HIGH disables the TB6600 output stage; LOW keeps holding torque.
constexpr uint8_t ENABLE_LEVEL = LOW;

volatile uint32_t pendingSteps = 0;
volatile bool requestPending = false;
volatile bool stopRequested = false;
volatile bool running = false;
volatile bool busy = false;
volatile bool moveForward = true;
volatile int32_t position = 0;
}  // namespace

void stepper_init() {
    pinMode(STEPPER_STEP_GPIO, OUTPUT);
    pinMode(STEPPER_DIR_GPIO, OUTPUT);
    pinMode(STEPPER_EN_GPIO, OUTPUT);
    digitalWrite(STEPPER_STEP_GPIO, LOW);
    digitalWrite(STEPPER_DIR_GPIO, LOW);
    digitalWrite(STEPPER_EN_GPIO, ENABLE_LEVEL);
}

void stepper_start(bool forward, uint32_t durationMs) {
    const uint64_t steps =
        static_cast<uint64_t>(durationMs) * STEPPER_STEPS_PER_SEC / 1000;
    moveForward = forward;
    pendingSteps = static_cast<uint32_t>(steps);
    stopRequested = false;
    running = true;
    requestPending = true;
}

void stepper_stop() {
    requestPending = false;
    stopRequested = true;
    // The step loop checks stopRequested before every pulse.
    while (busy) {
        delayMicroseconds(50);
    }
    running = false;
}

bool stepper_is_running() {
    return running;
}

int32_t stepper_position() {
    return position;
}

// Busy-waits for accurate pulse timing on core 0 at idle priority; the idle
// task yields straight back, so the watchdog stays fed without long gaps.
void task_stepper(void *parameter) {
    while (true) {
        if (!requestPending) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        busy = true;
        requestPending = false;

        const bool forward = moveForward;
        const uint32_t total = pendingSteps;
        digitalWrite(STEPPER_DIR_GPIO, forward ? HIGH : LOW);
        delayMicroseconds(DIR_SETUP_US);

        // Trapezoidal profile: v^2 = v0^2 + 2*a*s, mirrored for deceleration.
        const float v0 = STEPPER_START_STEPS_PER_SEC;
        const float vMax = STEPPER_STEPS_PER_SEC;
        const float a2 = 2.0f * STEPPER_ACCEL_STEPS_PER_SEC2;
        const float v0Sq = v0 * v0;

        uint32_t lastStepUs = micros();
        uint32_t intervalUs = static_cast<uint32_t>(1e6f / v0);

        for (uint32_t done = 0; done < total; ++done) {
            if (stopRequested) {
                break;
            }
            while (static_cast<uint32_t>(micros() - lastStepUs) < intervalUs) {
            }
            lastStepUs = micros();
            digitalWrite(STEPPER_STEP_GPIO, HIGH);
            delayMicroseconds(STEP_PULSE_US);
            digitalWrite(STEPPER_STEP_GPIO, LOW);
            position += forward ? 1 : -1;

            const uint32_t fromEdge = min(done + 1, total - done - 1);
            float v = sqrtf(v0Sq + a2 * fromEdge);
            if (v > vMax) {
                v = vMax;
            }
            intervalUs = static_cast<uint32_t>(1e6f / v);
        }

        running = false;
        busy = false;
    }
}
