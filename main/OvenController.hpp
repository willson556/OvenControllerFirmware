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
        Off, BeginPreheat, Preheating, On
    };

    typedef void (*StateChangedHandler)(State newState);

    OvenController(
        gpio_num_t bakeButton,
        gpio_num_t startButton,
        gpio_num_t incrementButton,
        gpio_num_t decrementButton,
        gpio_num_t cancelButton,
        gpio_num_t bakeCoilSense,
        const char *name,
        StateChangedHandler stateChangedHandler)
        : bakeButton{bakeButton},
          startButton{startButton},
          incrementButton{incrementButton},
          decrementButton{decrementButton},
          cancelButton{cancelButton},
          buttons{bakeButton, startButton, incrementButton, decrementButton, cancelButton},
          bakeCoilSense{bakeCoilSense},
          name{name},
          stateChangedHandler{stateChangedHandler}
    {   
    }

    /**
     * @brief Syncs this controller with the oven's state. MUST be called before calling any other instance methods.
     * 
     */
    void initialize();

    void setTemperatureFahrenheit(float f);
    
    void setTemperatureCelsius(float c) {
        setTemperatureFahrenheit(CelsiusToFahrenheit(c));
    }

    float getTemperatureInCelsius() {
        return FahrenheitToCelsius(currentSetpoint);
    }

    float getTemperatureInFahrenheit() {
        return currentSetpoint;
    }

    State getCurrentState() {
        return currentState;
    }

    void turnOn();
    void turnOff();
    void resync();

    friend void OvenControllerInternals::ovenTask(void *pvParameters);

    float getMaxTemperatureInFahrenheit() { return MaxTemperature; }
    float getMaxTemperatureInCelsius() { return FahrenheitToCelsius(MaxTemperature); }
    float getMinTemperatureInFahrenheit() { return MinTemperature; }
    float getMinTemperatureInCelsius() { return FahrenheitToCelsius(MinTemperature); }
    float getTemperatureStepSizeInFahrenheit() { return TemperatureStepSize; }
    float getTemperatureStepSizeInCelsius() { return FahrenheitToCelsius(TemperatureStepSize); }

private:
    void setState(State newState)
    {
        currentState = newState;
        stateChanged = true;
    }

    void task();
    TaskHandle_t taskHandle{nullptr};

    volatile float currentSetpoint {350}; // degF
    volatile float targetSetpoint {350}; // degF
    volatile State currentState {State::Off};
    
    volatile bool turnOnRequest {false};
    volatile bool turnOffRequest {false};
    volatile bool stateChanged {false};
    
    static constexpr size_t num_buttons = 5;
    const gpio_num_t bakeButton;
    const gpio_num_t startButton;
    const gpio_num_t incrementButton;
    const gpio_num_t decrementButton;
    const gpio_num_t cancelButton;

    const array<gpio_num_t, num_buttons> buttons;

    const gpio_num_t bakeCoilSense;
    const char* name;
    const StateChangedHandler stateChangedHandler;

    static constexpr float MaxTemperature = 500; // degF
    static constexpr float MinTemperature = 200; // degF
    static constexpr float TemperatureStepSize = 5; //degF
};
