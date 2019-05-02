#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "hap.h"
#include "config.h"

#include "udp_logging.h"

#include "OvenController.hpp"


#define TAG "Oven"
#define ACCESSORY_NAME  TAG
#define MANUFACTURER_NAME   "TEW"
#define MODEL_NAME  "v1.1"
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static void* a;
static void* current_heating_cooling_event_handle = nullptr;
static void* target_heating_cooling_event_handle = nullptr;
static void* current_temperature_event_handle = nullptr;
static void* target_temperature_event_handle = nullptr;
static void* temperature_display_units_event_handle = nullptr;

static void* temperatureDisplayUnits = 0;

constexpr gpio_num_t bakeButton = GPIO_NUM_16;
constexpr gpio_num_t startButton = GPIO_NUM_19;
constexpr gpio_num_t incrementButton = GPIO_NUM_25;
constexpr gpio_num_t decrementButton = GPIO_NUM_32;
constexpr gpio_num_t cancelButton = GPIO_NUM_33;
constexpr gpio_num_t bakeCoilSense = GPIO_NUM_5;

constexpr int OvenOffTemperature = 20; // degC

void StateChangedHandler(OvenController::State newState);

static OvenController ovenController { bakeButton, startButton, incrementButton, decrementButton, cancelButton, bakeCoilSense, "OvenController", StateChangedHandler };

void* intToFloatVoidPtr(int value)
{
    return (void*)(value * 100); // This casts the floating point format data to a void*.
}

int floatVoidPtrToInt(void* ptr)
{
    return ((int)ptr); // This converts the float to int.
}

void* getCurrentHeatingCoolingState(void* arg) { 
    switch (ovenController.getCurrentState())
    {
        case OvenController::State::On:
        case OvenController::State::Preheat:
            return (void*)1;
        case OvenController::State::Off:
        default:
            return (void*)0;
    }
 }

 void setCurrentHeatingCoolingStateEventHandle(void* arg, void* ev_handle, bool enable)
 {
     if (enable) {
         current_heating_cooling_event_handle = ev_handle;
     } else {
         current_heating_cooling_event_handle = nullptr;
     }
 }

 void setTargetHeatingCoolingState(void* arg, void* value, int len)
 {
     switch ((int)value)
     {
         case 0:
             ovenController.turnOff();
             break;
        case 1:
        default:
            ovenController.turnOn();
            break;
     }

    if (current_heating_cooling_event_handle) {
        hap_event_response(a, current_heating_cooling_event_handle, value);
    }

    if (target_heating_cooling_event_handle) {
        hap_event_response(a, target_heating_cooling_event_handle, value);
    }
 }

void setTargetHeatingCoolingStateEventHandle(void* arg, void* ev_handle, bool enable)
{
    if (enable) {
        target_heating_cooling_event_handle = ev_handle;
    } else {
        target_heating_cooling_event_handle = nullptr;
    }
}

void* getCurrentTemperature(void *arg)
{
    switch (ovenController.getCurrentState())
    {
        case OvenController::State::Off:
            return intToFloatVoidPtr(OvenOffTemperature);
        case OvenController::State::Preheat:
            return intToFloatVoidPtr(ovenController.getTemperatureInCelsius() / 2);
        case OvenController::State::On:
        default:
            return intToFloatVoidPtr(ovenController.getTemperatureInCelsius());
    }
}

void setCurrentTemperatureEventHandle(void* arg, void* ev_handle, bool enable)
{
    current_temperature_event_handle = enable ? ev_handle : nullptr;
}

void* getTargetTemperature(void *arg)
{
    return intToFloatVoidPtr(ovenController.getTemperatureInCelsius());
}

void setTargetTemperature(void* arg, void* value, int len)
{
    auto temperature = floatVoidPtrToInt(value);
    ovenController.setTemperatureCelsius(temperature);

    if (target_temperature_event_handle) {
        hap_event_response(a, target_temperature_event_handle, value);
    }
}

void setTargetTemperatureEventHandle(void* arg, void* ev_handle, bool enable)
{
    target_temperature_event_handle = enable ? ev_handle : nullptr;
}

void* getDisplayUnits(void *arg)
{
    return temperatureDisplayUnits;
}

void setDisplayUnits(void* arg, void* value, int len)
{
    temperatureDisplayUnits = value;

    if (temperature_display_units_event_handle) {
        hap_event_response(a, temperature_display_units_event_handle, value);
    }
}

void setDisplayUnitsEventHandle(void* arg, void* ev_handle, bool enable)
{
    temperature_display_units_event_handle = enable ? ev_handle : nullptr;
}

void StateChangedHandler(OvenController::State newState)
{
    if (current_heating_cooling_event_handle) {
        hap_event_response(a, current_heating_cooling_event_handle, getCurrentHeatingCoolingState(nullptr));
    }

    if (current_temperature_event_handle) {
        hap_event_response(a, current_temperature_event_handle, getCurrentTemperature(nullptr));
    }

    if (target_heating_cooling_event_handle) {
        hap_event_response(a, target_heating_cooling_event_handle, getCurrentHeatingCoolingState(nullptr));
    }
}

void* identify_read(void* arg)
{
    return (void*)true;
}

void hap_object_init(void* arg)
{
    void* accessory_object = hap_accessory_add(a);
    struct hap_characteristic cs[] = {
        {HAP_CHARACTER_IDENTIFY, (void*)true, NULL, identify_read, NULL, NULL},
        {HAP_CHARACTER_MANUFACTURER, (void*)MANUFACTURER_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_MODEL, (void*)MODEL_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_NAME, (void*)ACCESSORY_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_SERIAL_NUMBER, (void*)"8548851", NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_FIRMWARE_REVISION, (void*)"1.0", NULL, NULL, NULL, NULL},
    };
    hap_service_and_characteristics_add(a, accessory_object, HAP_SERVICE_ACCESSORY_INFORMATION, cs, ARRAY_SIZE(cs));

    struct hap_characteristic_ex state[] = {
        {HAP_CHARACTER_CURRENT_HEATING_COOLING_STATE, getCurrentHeatingCoolingState(nullptr), nullptr, getCurrentHeatingCoolingState, nullptr, setCurrentHeatingCoolingStateEventHandle, false, nullptr, false, nullptr},
        {HAP_CHARACTER_TARGET_HEATING_COOLING_STATE, getCurrentHeatingCoolingState(nullptr), nullptr, getCurrentHeatingCoolingState, setTargetHeatingCoolingState, setTargetHeatingCoolingStateEventHandle, false, nullptr, false, nullptr},
        {HAP_CHARACTER_CURRENT_TEMPERATURE, getCurrentTemperature(nullptr), nullptr, getCurrentTemperature, nullptr, setCurrentTemperatureEventHandle, true, intToFloatVoidPtr(ovenController.getMaxTemperatureInCelsius()), true, intToFloatVoidPtr(OvenOffTemperature)},
        {HAP_CHARACTER_TARGET_TEMPERATURE, getTargetTemperature(nullptr), nullptr, getTargetTemperature, setTargetTemperature, setTargetTemperatureEventHandle, true, intToFloatVoidPtr(ovenController.getMaxTemperatureInCelsius()), true, intToFloatVoidPtr(ovenController.getMinTemperatureInCelsius())},
        {HAP_CHARACTER_TEMPERATURE_DISPLAY_UNITS, getDisplayUnits(nullptr), nullptr, getDisplayUnits, setDisplayUnits, setDisplayUnitsEventHandle, false, nullptr, false, nullptr},
    };
    hap_service_and_characteristics_ex_add(a, accessory_object, HAP_SERVICE_THERMOSTAT, state, ARRAY_SIZE(state));
}

static void network_logging_init()
{
    udp_logging_init(CONFIG_LOG_UDP_IP, CONFIG_LOG_UDP_PORT, udp_logging_vprintf);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        {
            network_logging_init();
            hap_init();

            uint8_t mac[6];
            esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
            char accessory_id[32] = {0,};
            sprintf(accessory_id, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            hap_accessory_callback_t callback;
            callback.hap_object_init = hap_object_init;
            a = hap_accessory_register((char*)ACCESSORY_NAME, accessory_id, (char*)"053-58-198", (char*)MANUFACTURER_NAME, HAP_ACCESSORY_CATEGORY_OTHER, 812, 1, NULL, &callback);
        }
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta()
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            {.ssid = MAIN_WIFI_SSID},
            {.password = MAIN_WIFI_PASSWORD}
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             MAIN_WIFI_SSID, MAIN_WIFI_PASSWORD);
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    //ESP_ERROR_CHECK( nvs_flash_erase() );
    ovenController.initialize();

    wifi_init_sta();
}
