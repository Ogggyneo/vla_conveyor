#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "global.h"

// TB6600 wired common-cathode (PUL-/DIR-/ENA- to GND), see mortorwiretodriver.png.
// YoloUNO D2/D3/D4 -> ESP32-S3 GPIO5/GPIO6/GPIO7.
#define STEPPER_STEP_GPIO 5
#define STEPPER_DIR_GPIO 6
#define STEPPER_EN_GPIO 7

// Driver DIP SW1-3 = 1600 pulses/rev.
constexpr uint32_t STEPPER_STEPS_PER_REV = 1600;
// Cruise rate used to convert MOVE duration into a step count (2 rev/s).
constexpr uint32_t STEPPER_STEPS_PER_SEC = 3200;
constexpr uint32_t STEPPER_START_STEPS_PER_SEC = 400;
constexpr uint32_t STEPPER_ACCEL_STEPS_PER_SEC2 = 16000;

void stepper_init();
void task_stepper(void *parameter);

// Starts a move of exactly durationMs * STEPPER_STEPS_PER_SEC / 1000 steps.
void stepper_start(bool forward, uint32_t durationMs);
void stepper_stop();
bool stepper_is_running();
int32_t stepper_position();

#endif
