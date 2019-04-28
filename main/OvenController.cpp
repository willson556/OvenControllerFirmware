
#include "OvenController.hpp"

namespace
{
constexpr TickType_t button_press_delay = 200; // milliseconds

enum GPIOState
{
    OFF = 0,
    ON = 1
};

template <TickType_t milliseconds>
void delay()
{
    vTaskDelay(milliseconds / portTICK_PERIOD_MS);
}

void pressButton(gpio_num_t button)
{
    printf("Push button %d\n", button);
    gpio_set_level(button, ON);
    delay<button_press_delay>();
    gpio_set_level(button, OFF);
    delay<button_press_delay>();
}
} // namespace

constexpr int defaultTemperature = 350;
constexpr int stepSize = 5;

void OvenControllerInternals::ovenTask(void *pvParameters)
{
    static_cast<OvenController *>(pvParameters)->task();
}

void OvenController::setTemperatureFahrenheit(int f)
{
    targetSetpoint = f;
}

void OvenController::initialize()
{
    for (auto button : buttons)
    {
        gpio_pad_select_gpio(button);
        gpio_set_direction(button, GPIO_MODE_OUTPUT);
        gpio_set_level(button, OFF);
    }

    resync();

    auto taskResult = xTaskCreate(
        OvenControllerInternals::ovenTask,
        name,
        3000,
        this,
        tskIDLE_PRIORITY,
        &taskHandle);

    assert(taskResult == pdPASS);
}

void OvenController::turnOn()
{
    turnOnRequest = true;
}

void OvenController::turnOff()
{
    turnOffRequest = true;
}

void OvenController::resync()
{
    pressButton(cancelButton);
    setState(State::Off);
}

void OvenController::task()
{
    constexpr long offTickCountPreheatThreshold = pdMS_TO_TICKS(30000);
    auto sawBakeCoilOn = false;
    long offTickStart = 0;
    bool offTickStartCaptured = false;

    while (true)
    {
        auto bakeCoilState = !gpio_get_level(bakeCoilSense);
        printf("Bake Coil Sense State: %d\n", bakeCoilState);
        printf("Current State: %d\n", (int)currentState);

        if (bakeCoilState && currentState == State::Off)
        {
            printf("Oven turned on by person!\n");
            setState(State::Preheat);             // Someone must have manually operated the oven.
            currentSetpoint = defaultTemperature; // We don't really know the temperature but we'll put something in.
        }
        else if (bakeCoilState && currentState == State::Preheat)
        {
            sawBakeCoilOn = true;
            offTickStartCaptured = false;
        }
        else if (!bakeCoilState && sawBakeCoilOn && currentState == State::Preheat)
        {
            if (!offTickStartCaptured)
            {
                offTickStartCaptured = true;
                offTickStart = xTaskGetTickCount();
            }
            else if (xTaskGetTickCount() - offTickStart > offTickCountPreheatThreshold)
            {
                printf("Preheat finished!\n");

                // Seems the coil went off for more than 30 seconds, most likely done pre-heating.
                // Other possibility is the user accessed the control panel.
                setState(State::On);
                offTickStartCaptured = false;
            }
        }

        if (targetSetpoint != currentSetpoint)
        {
            printf("New setpoint of %dF\n", targetSetpoint);
            currentSetpoint = targetSetpoint;

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

        if (turnOffRequest)
        {
            printf("Turning off oven\n");
            pressButton(cancelButton);
            turnOffRequest = false;
            setState(State::Off);
        }

        if (turnOnRequest)
        {
            if (currentState == State::On || currentState == State::Preheat)
            {
                turnOnRequest = false;
                return;
            }

            printf("Turning on oven\n");
            offTickStartCaptured = false;
            sawBakeCoilOn = false;
            pressButton(bakeButton);

            if (currentSetpoint > defaultTemperature)
            {
                for (int ovenSetpoint = defaultTemperature; ovenSetpoint < currentSetpoint; ovenSetpoint += stepSize)
                {
                    pressButton(incrementButton);
                }
            }
            else if (currentSetpoint < defaultTemperature)
            {
                for (int ovenSetpoint = defaultTemperature; ovenSetpoint > currentSetpoint; ovenSetpoint -= stepSize)
                {
                    pressButton(decrementButton);
                }
            }

            pressButton(startButton);

            turnOnRequest = false;
            setState(State::Preheat);
        }

        if (stateChanged)
        {
            stateChanged = false;

            if (stateChangedHandler)
            {
                stateChangedHandler(currentState);
            }
        }

        delay<1000>();
    }
}