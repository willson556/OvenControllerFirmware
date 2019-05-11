#pragma once

#include <functional>
#include <vector>
#include <string>

#include "hap.hpp"
#include "OvenController.hpp"
#include "config.h"

class OvenControllerHomeKitBridge : public HAP::Accessory
{
public:
    OvenControllerHomeKitBridge(std::string &id, OvenController &ovenController)
        : Accessory(accessory_name,
                    id,
                    setup_code,
                    manufacturer_name,
                    firmware_version,
                    model,
                    serial_number,
                    HAP_ACCESSORY_CATEGORY_OTHER,
                    port,
                    HOMEKIT_CONFIG_NUMBER),
          ovenController{ovenController},
          target_heating_cooling{ovenController}
    {
        ovenController.addHeatingElementListener([this](bool){ current_heating_cooling.notify(); });
        ovenController.addStateListener([this](OvenController::State){ current_temperature.notify(); });
    }

protected:
    void init() override
    {
        add_service(HAP_SERVICE_THERMOSTAT, characteristics);
    }

private:
    OvenController &ovenController;
    const std::string accessory_name {"Oven"};
    const std::string setup_code {"053-58-198"};
    const std::string manufacturer_name {"TEW"};
    const std::string firmware_version {CURRENT_VERSION};
    const std::string model {"v1.1"};
    const std::string serial_number {"8548851"};

    class TargetHeatingCoolingCharacteristic : public HAP::IntCharacteristic
    {
    public:
        TargetHeatingCoolingCharacteristic(OvenController &ovenController)
            : IntCharacteristic{ HAP_CHARACTER_TARGET_HEATING_COOLING_STATE },
              ovenController{ovenController}
        {
            ovenController.addStateListener([this](OvenController::State state){ notify(); });
        }

    protected:
        int readInt() const override
        {
            switch (ovenController.getCurrentState())
            {
            case OvenController::State::On:
            case OvenController::State::BeginPreheat:
            case OvenController::State::Preheating:
                return 1;
            case OvenController::State::Off:
            default:
                return 0;
            }
        }

        void writeInt(int value) override
        {
            switch (value)
            {
            case 0:
                ovenController.turnOff();
                break;
            case 1:
            default:
                ovenController.turnOn();
                break;
            }
        }

        bool canRead() const override { return true; }
        bool canWrite() const override { return true; }

        std::tuple<bool, std::vector<int>> get_valid_values_override() const override
        {
            return std::make_tuple(true, std::vector<int>{ 0, 1 });
        }
    
    private:
        OvenController &ovenController;
    };

    template<int MIN_TEMP, int MAX_TEMP>
    class TemperatureCharacteristic : public HAP::FloatFunctionCharacteristic
    {
    public:
        using FloatFunctionCharacteristic::FloatFunctionCharacteristic;
    
    protected:
        std::tuple<bool, float> get_max_value_override_float() const override
        {
            return std::make_tuple(true, MAX_TEMP);
        }

        std::tuple<bool, float> get_min_value_override_float() const override
        {
            return std::make_tuple(true, MIN_TEMP);
        }
    };

    HAP::IntFunctionCharacteristic current_heating_cooling
    {
        HAP_CHARACTER_CURRENT_HEATING_COOLING_STATE,
        [this]()
        {
            return ovenController.getHeatingElementState();
        },
        std::function<void(int)>{}
    };

    TargetHeatingCoolingCharacteristic target_heating_cooling;

    TemperatureCharacteristic<94, 260> target_temperature
    {
        HAP_CHARACTER_TARGET_TEMPERATURE,
        [this]() { return ovenController.getTemperatureInCelsius(); },
        [this](float value) { ovenController.setTemperatureCelsius(value); }
    };

    TemperatureCharacteristic<20, 260> current_temperature
    {
        HAP_CHARACTER_CURRENT_TEMPERATURE,
        [this]()
        {
            switch(ovenController.getCurrentState())
            {
            case OvenController::State::Off:
            default:
                return off_temperature;
            case OvenController::State::BeginPreheat:
            case OvenController::State::Preheating:
                return ovenController.getTemperatureInCelsius() / 2.0f;
            case OvenController::State::On:
                return ovenController.getTemperatureInCelsius();
            }
        },
        std::function<void(float)>{},
    };

    HAP::IntFunctionCharacteristic display_units_characteristic
    {
        HAP_CHARACTER_TEMPERATURE_DISPLAY_UNITS,
        [this]() {return display_units;},
        [this](int value) { display_units = value; },
    };

    const std::vector<HAP::Characteristic*> characteristics {
        &current_heating_cooling,
        &target_heating_cooling,
        &target_temperature,
        &current_temperature,
        &display_units_characteristic,
    };

    int display_units {0};
    static constexpr int port = 812;
    static constexpr float off_temperature = 20; // degC
};
