
#include "OvenController.hpp"

namespace {
    constexpr TickType_t button_press_delay = 250; // milliseconds

    enum GPIOState {
        OFF = 0, ON = 1
    };

    template <TickType_t milliseconds>
    void delay() {
        vTaskDelay(milliseconds / portTICK_PERIOD_MS);
    }

    void pressButton(gpio_num_t button) {
        gpio_set_level(button, ON);
        delay<button_press_delay>();
        gpio_set_level(button, OFF);
    }
}

constexpr int defaultTemperature = 350;
constexpr int stepSize = 5;

void OvenControllerInternals::ovenTask(void *pvParameters) {
    static_cast<OvenController*>(pvParameters)->task();
}

void OvenController::setTemperatureFahrenheit(int f) {
    currentSetpoint = f;

    switch (currentState)
    {
        case State::On:
        case State::Preheat:
            turnOff();
            turnOn();
            break;
        case State::Off:
        default:
            break;
    }
}

void OvenController::initialize() {
    for (auto button: buttons) {
        gpio_pad_select_gpio(button);
        gpio_set_direction(button, GPIO_MODE_OUTPUT);
        gpio_set_level(button, OFF);
    }

    resync();

    auto taskResult = xTaskCreate(
        OvenControllerInternals::ovenTask,
        name,
        250,
        this,
        tskIDLE_PRIORITY,
        &taskHandle
    );

    assert(taskResult == pdPASS);
}

void OvenController::turnOn() {
    if (currentState == State::On || currentState == State::Preheat) {
        return;
    }

    pressButton(bakeButton);

    if (currentSetpoint == defaultTemperature) {
        return;
    } else if (currentSetpoint > defaultTemperature) {
        for (int ovenSetpoint = defaultTemperature; ovenSetpoint < currentSetpoint; ovenSetpoint += stepSize) {
            pressButton(incrementButton);
        }
    } else {
        for (int ovenSetpoint = defaultTemperature; ovenSetpoint > currentSetpoint; ovenSetpoint -= stepSize) {
            pressButton(decrementButton);
        }
    }
}

void OvenController::turnOff() {
    pressButton(cancelButton);
}

void OvenController::resync() {
    pressButton(cancelButton);
    currentState = State::Off;
}

void OvenController::task() {
    if (gpio_get_level(bakeCoilSense) && currentState == State::Off) {
        currentState = State::Preheat; // Someone must have manually operated the oven.
        currentSetpoint = defaultTemperature; // We don't really know the temperature but we'll put something in.
    } else if (currentState == State::Preheat) {
        // Seems the coil went off, most likely done pre-heating.
        // Other possibility is the user accessed the control panel.
        currentState = State::On;
    }
}