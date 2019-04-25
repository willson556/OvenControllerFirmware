#pragma once

#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/gpio.h>
#include <assert.h>

#include "units.hpp"

using namespace std;

class OvenController;

namespace OvenControllerInternals {
    void ovenTask(void *pvParameters);
}

class OvenController {
public:
    enum class State {
        Off, Preheat, On
    };

    OvenController(
        gpio_num_t bakeButton,
        gpio_num_t startButton,
        gpio_num_t incrementButton,
        gpio_num_t decrementButton,
        gpio_num_t cancelButton,
        gpio_num_t bakeCoilSense,
        const char *name)
        : bakeButton{bakeButton},
          startButton{startButton},
          incrementButton{incrementButton},
          decrementButton{decrementButton},
          cancelButton{cancelButton},
          buttons{bakeButton, startButton, incrementButton, decrementButton, cancelButton},
          bakeCoilSense{bakeCoilSense},
          name{name}
    {   
    }

    /**
     * @brief Syncs this controller with the oven's state. MUST be called before calling any other instance methods.
     * 
     */
    void initialize();

    void setTemperatureFahrenheit(int f);
    
    void setTemperatureCelsius(int c) {
        setTemperatureFahrenheit(CelsiusToFahrenheit(c));
    }

    int getTemperatureInCelsius() {
        return FahrenheitToCelsius(currentSetpoint);
    }

    int getTemperatureInFahrenheit() {
        return currentSetpoint;
    }

    State getCurrentState() {
        return currentState;
    }

    void turnOn();
    void turnOff();
    void resync();

    friend void OvenControllerInternals::ovenTask(void *pvParameters);


private:
    void task();
    TaskHandle_t taskHandle{nullptr};

    int currentSetpoint {350};
    State currentState {State::Off};

    static constexpr size_t num_buttons = 5;
    const gpio_num_t bakeButton;
    const gpio_num_t startButton;
    const gpio_num_t incrementButton;
    const gpio_num_t decrementButton;
    const gpio_num_t cancelButton;

    const array<gpio_num_t, num_buttons> buttons;

    const gpio_num_t bakeCoilSense;
    const char* name;
};
