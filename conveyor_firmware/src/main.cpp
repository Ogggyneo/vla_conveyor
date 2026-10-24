#include "global.h"
#include "stepper_motor.h"
#include "driver/gpio.h"

namespace {
// Đổi thành false nếu lệnh RIGHT làm vật chạy sang trái.
constexpr bool IMAGE_RIGHT_IS_FORWARD = false;

constexpr uint32_t MIN_DURATION_MS = 50;
constexpr uint32_t MAX_DURATION_MS = 10000;

bool motorRunning = false;

void stopMotor() {
    stepper_stop();
    motorRunning = false;
}

void startMove(bool moveRight, uint32_t durationMs) {
    const bool forward = moveRight
        ? IMAGE_RIGHT_IS_FORWARD
        : !IMAGE_RIGHT_IS_FORWARD;

    stepper_stop();
    stepper_start(forward, durationMs);
    motorRunning = true;
}

void handleCommand(String command) {
    command.trim();

    if (command == "PING") {
        Serial.println("PONG");
        return;
    }

    if (command == "STOP") {
        stopMotor();
        Serial.println("STOPPED");
        return;
    }

    if (command == "PINTEST") {
        // Holds STEP (D2) and DIR (D3) HIGH for a multimeter check.
        stopMotor();
        // INPUT_OUTPUT lets the pad level be read back: LOW means an external
        // short/load is pulling the pin down.
        gpio_set_direction(static_cast<gpio_num_t>(STEPPER_STEP_GPIO), GPIO_MODE_INPUT_OUTPUT);
        gpio_set_direction(static_cast<gpio_num_t>(STEPPER_DIR_GPIO), GPIO_MODE_INPUT_OUTPUT);
        digitalWrite(STEPPER_STEP_GPIO, HIGH);
        digitalWrite(STEPPER_DIR_GPIO, HIGH);
        Serial.println("PINTEST,HIGH");
        delay(50);
        Serial.printf(
            "PINTEST,READBACK,STEP=%d,DIR=%d,EN=%d\n",
            gpio_get_level(static_cast<gpio_num_t>(STEPPER_STEP_GPIO)),
            gpio_get_level(static_cast<gpio_num_t>(STEPPER_DIR_GPIO)),
            digitalRead(STEPPER_EN_GPIO)
        );
        delay(30000);
        digitalWrite(STEPPER_STEP_GPIO, LOW);
        digitalWrite(STEPPER_DIR_GPIO, LOW);
        Serial.println("PINTEST,LOW");
        return;
    }

    if (command == "POS") {
        Serial.printf("POS,%ld\n", static_cast<long>(stepper_position()));
        return;
    }

    char direction[8] = {0};
    unsigned long durationMs = 0;

    const int parsed = sscanf(
        command.c_str(),
        "MOVE,%7[^,],%lu",
        direction,
        &durationMs
    );

    if (parsed != 2) {
        Serial.println("ERR,BAD_FORMAT");
        return;
    }

    const bool moveRight = strcmp(direction, "RIGHT") == 0;
    const bool moveLeft = strcmp(direction, "LEFT") == 0;

    if (!moveRight && !moveLeft) {
        Serial.println("ERR,BAD_DIRECTION");
        return;
    }

    durationMs = constrain(
        durationMs,
        static_cast<unsigned long>(MIN_DURATION_MS),
        static_cast<unsigned long>(MAX_DURATION_MS)
    );

    startMove(moveRight, durationMs);
    Serial.printf("ACK,%s,%lu\n", direction, durationMs);
}
}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(40);
    delay(1000);

    stepper_init();
    // The stepper task busy-waits on core 0 at idle priority, so the idle task
    // (configIDLE_SHOULD_YIELD) still feeds the watchdog; loop() stays on core 1.
    xTaskCreatePinnedToCore(
        task_stepper,
        "stepper",
        4096,
        nullptr,
        tskIDLE_PRIORITY,
        nullptr,
        0
    );

    Serial.println("READY");
}

void loop() {
    if (
        motorRunning
        && !stepper_is_running()
    ) {
        motorRunning = false;
        Serial.println("DONE");
    }

    if (Serial.available() > 0) {
        handleCommand(Serial.readStringUntil('\n'));
    }

    delay(2);
}
