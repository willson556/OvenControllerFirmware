#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "OvenController.hpp"

namespace
{

static const char* TAG = "OvenController";
constexpr TickType_t button_press_delay = 200; // milliseconds

enum GPIOState
{
    OFF = 0,
    ON = 1
};

template <TickType_t milliseconds>
static void delay()
{
    vTaskDelay(milliseconds / portTICK_PERIOD_MS);
}

void pressButton(gpio_num_t button)
{
    ESP_LOGD(TAG, "Push button %d\n", button);
    gpio_set_level(button, ON);
    delay<button_press_delay>();
    gpio_set_level(button, OFF);
    delay<button_press_delay>();
}
} // namespace

constexpr float defaultTemperature = 350;
constexpr float stepSize = 5;

void OvenControllerInternals::ovenTask(void *pvParameters)
{
    static_cast<OvenController *>(pvParameters)->task();
}

void OvenController::setTemperatureFahrenheit(float f)
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
    int memoryCountDivider = 0;

    while (true)
    {
        auto bakeCoilState = !gpio_get_level(bakeCoilSense);

        if (bakeCoilState && currentState == State::Off)
        {
            ESP_LOGD(TAG, "Oven turned on by person!\n");  // Someone must have manually operated the oven.
            targetSetpoint = currentSetpoint = defaultTemperature; // We don't really know the temperature but we'll put something in.
            setState(State::Preheat);
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
                ESP_LOGD(TAG, "Preheat finished!\n");

                // Seems the coil went off for more than 30 seconds, most likely done pre-heating.
                // Other possibility is the user accessed thecontrol panel.
                setState(State::On);
                offTickStartCaptured = false;
            }
        }

        if (targetSetpoint != currentSetpoint)
        {
            ESP_LOGD(TAG, "New setpoint of %fF\n", targetSetpoint);
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
            ESP_LOGD(TAG, "Turning off oven\n");
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

            ESP_LOGD(TAG, "Turning on oven\n");
            offTickStartCaptured = false;
            sawBakeCoilOn = false;
            pressButton(bakeButton);

            if (currentSetpoint > defaultTemperature)
            {
                for (float ovenSetpoint = defaultTemperature; ovenSetpoint < currentSetpoint - stepSize / 2.0; ovenSetpoint += stepSize)
                {
                    pressButton(incrementButton);
                }
            }
            else if (currentSetpoint < defaultTemperature)
            {
                for (float ovenSetpoint = defaultTemperature; ovenSetpoint > currentSetpoint + stepSize / 2.0; ovenSetpoint -= stepSize)
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

        if ((memoryCountDivider++ % 60) == 0) {
            ESP_LOGI(TAG, "Available Memory: %d\n", xPortGetFreeHeapSize());
        }

        delay<1000>();
    }
}