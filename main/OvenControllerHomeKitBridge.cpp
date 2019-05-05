#include "OvenControllerHomeKitBridge.hpp"
#include "hap.h"
#include "config.h"

constexpr int OvenOffTemperature = 20; // degC

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static void *a;
static void *current_heating_cooling_event_handle = nullptr;
static void *target_heating_cooling_event_handle = nullptr;
static void *current_temperature_event_handle = nullptr;
static void *target_temperature_event_handle = nullptr;
static void *temperature_display_units_event_handle = nullptr;

static void *temperatureDisplayUnits = 0;

static OvenController *ovenController = nullptr;

void hap_object_init(void *arg);

void hap_oven_initialize(OvenController &o, uint8_t *mac)
{
    ovenController = &o;
    hap_init();

    char accessory_id[32] = {
        0,
    };
    sprintf(accessory_id, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    hap_accessory_callback_t callback;
    callback.hap_object_init = hap_object_init;

    a = hap_accessory_register((char *)ACCESSORY_NAME, accessory_id, (char *)"053-58-198", (char *)MANUFACTURER_NAME, HAP_ACCESSORY_CATEGORY_OTHER, 812, HOMEKIT_CONFIG_NUMBER, NULL, &callback);
}

void *intToFloatVoidPtr(float value)
{
    return (void *)(int)(value * 100.0); // This casts the floating point format data to a void*.
}

float floatVoidPtrToInt(void *ptr)
{
    return ((int)ptr) / 100.0; // This converts the int to float.
}

void *getCurrentHeatingCoolingState(void *arg)
{
    switch (ovenController->getCurrentState())
    {
    case OvenController::State::On:
    case OvenController::State::Preheat:
        return (void *)1;
    case OvenController::State::Off:
    default:
        return (void *)0;
    }
}

void setCurrentHeatingCoolingStateEventHandle(void *arg, void *ev_handle, bool enable)
{
    if (enable)
    {
        current_heating_cooling_event_handle = ev_handle;
    }
    else
    {
        current_heating_cooling_event_handle = nullptr;
    }
}

void setTargetHeatingCoolingState(void *arg, void *value, int len)
{
    switch ((int)value)
    {
    case 0:
        ovenController->turnOff();
        break;
    case 1:
    default:
        ovenController->turnOn();
        break;
    }

    if (current_heating_cooling_event_handle)
    {
        hap_event_response(a, current_heating_cooling_event_handle, value);
    }

    if (target_heating_cooling_event_handle)
    {
        hap_event_response(a, target_heating_cooling_event_handle, value);
    }
}

void setTargetHeatingCoolingStateEventHandle(void *arg, void *ev_handle, bool enable)
{
    if (enable)
    {
        target_heating_cooling_event_handle = ev_handle;
    }
    else
    {
        target_heating_cooling_event_handle = nullptr;
    }
}

void *getCurrentTemperature(void *arg)
{
    switch (ovenController->getCurrentState())
    {
    case OvenController::State::Off:
        return intToFloatVoidPtr(OvenOffTemperature);
    case OvenController::State::Preheat:
        return intToFloatVoidPtr(ovenController->getTemperatureInCelsius() / 2);
    case OvenController::State::On:
    default:
        return intToFloatVoidPtr(ovenController->getTemperatureInCelsius());
    }
}

void setCurrentTemperatureEventHandle(void *arg, void *ev_handle, bool enable)
{
    current_temperature_event_handle = enable ? ev_handle : nullptr;
}

void *getTargetTemperature(void *arg)
{
    return intToFloatVoidPtr(ovenController->getTemperatureInCelsius());
}

void setTargetTemperature(void *arg, void *value, int len)
{
    auto temperature = floatVoidPtrToInt(value);
    ovenController->setTemperatureCelsius(temperature);

    if (target_temperature_event_handle)
    {
        hap_event_response(a, target_temperature_event_handle, value);
    }
}

void setTargetTemperatureEventHandle(void *arg, void *ev_handle, bool enable)
{
    target_temperature_event_handle = enable ? ev_handle : nullptr;
}

void *getDisplayUnits(void *arg)
{
    return temperatureDisplayUnits;
}

void setDisplayUnits(void *arg, void *value, int len)
{
    temperatureDisplayUnits = value;

    if (temperature_display_units_event_handle)
    {
        hap_event_response(a, temperature_display_units_event_handle, value);
    }
}

void setDisplayUnitsEventHandle(void *arg, void *ev_handle, bool enable)
{
    temperature_display_units_event_handle = enable ? ev_handle : nullptr;
}

void hap_oven_StateChangedHandler(OvenController::State newState)
{
    if (current_heating_cooling_event_handle)
    {
        hap_event_response(a, current_heating_cooling_event_handle, getCurrentHeatingCoolingState(nullptr));
    }

    if (current_temperature_event_handle)
    {
        hap_event_response(a, current_temperature_event_handle, getCurrentTemperature(nullptr));
    }

    if (target_heating_cooling_event_handle)
    {
        hap_event_response(a, target_heating_cooling_event_handle, getCurrentHeatingCoolingState(nullptr));
    }
}

void *identify_read(void *arg)
{
    return (void *)true;
}

void hap_object_init(void *arg)
{
    void *accessory_object = hap_accessory_add(a);
    struct hap_characteristic cs[] = {
        {HAP_CHARACTER_IDENTIFY, (void *)true, NULL, identify_read, NULL, NULL},
        {HAP_CHARACTER_MANUFACTURER, (void *)MANUFACTURER_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_MODEL, (void *)MODEL_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_NAME, (void *)ACCESSORY_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_SERIAL_NUMBER, (void *)"8548851", NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_FIRMWARE_REVISION, (void *)CURRENT_VERSION, NULL, NULL, NULL, NULL},
    };
    hap_service_and_characteristics_add(a, accessory_object, HAP_SERVICE_ACCESSORY_INFORMATION, cs, ARRAY_SIZE(cs));

    struct hap_characteristic_ex state[] = {
        {HAP_CHARACTER_CURRENT_HEATING_COOLING_STATE, getCurrentHeatingCoolingState(nullptr), nullptr, getCurrentHeatingCoolingState, nullptr, setCurrentHeatingCoolingStateEventHandle, false, nullptr, false, nullptr, false, nullptr},
        {HAP_CHARACTER_TARGET_HEATING_COOLING_STATE, getCurrentHeatingCoolingState(nullptr), nullptr, getCurrentHeatingCoolingState, setTargetHeatingCoolingState, setTargetHeatingCoolingStateEventHandle, false, nullptr, false, nullptr, false, nullptr},
        {HAP_CHARACTER_CURRENT_TEMPERATURE, getCurrentTemperature(nullptr), nullptr, getCurrentTemperature, nullptr, setCurrentTemperatureEventHandle, true, intToFloatVoidPtr(ovenController->getMaxTemperatureInCelsius()), true, intToFloatVoidPtr(OvenOffTemperature), false, nullptr},
        {HAP_CHARACTER_TARGET_TEMPERATURE, getTargetTemperature(nullptr), nullptr, getTargetTemperature, setTargetTemperature, setTargetTemperatureEventHandle, true, intToFloatVoidPtr(ovenController->getMaxTemperatureInCelsius()), true, intToFloatVoidPtr(ovenController->getMinTemperatureInCelsius()), false, nullptr},
        {HAP_CHARACTER_TEMPERATURE_DISPLAY_UNITS, getDisplayUnits(nullptr), nullptr, getDisplayUnits, setDisplayUnits, setDisplayUnitsEventHandle, false, nullptr, false, nullptr, false, nullptr},
    };
    hap_service_and_characteristics_ex_add(a, accessory_object, HAP_SERVICE_THERMOSTAT, state, ARRAY_SIZE(state));
}