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

#include "config.h"

#include "udp_logging.h"

#include "OvenController.hpp"
#include "OvenControllerHomeKitBridge.hpp"

#include "ota.hpp"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

constexpr gpio_num_t bakeButton = GPIO_NUM_16;
constexpr gpio_num_t startButton = GPIO_NUM_19;
constexpr gpio_num_t incrementButton = GPIO_NUM_25;
constexpr gpio_num_t decrementButton = GPIO_NUM_32;
constexpr gpio_num_t cancelButton = GPIO_NUM_33;
constexpr gpio_num_t bakeCoilSense = GPIO_NUM_5;

static OvenController ovenController { bakeButton, startButton, incrementButton, decrementButton, cancelButton, bakeCoilSense, "OvenController", hap_oven_StateChangedHandler };
static HttpsOta::Ota ota { MAIN_UPDATE_FEED_URL, CURRENT_VERSION, [](){ ovenController.turnOff(); }};

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
            uint8_t mac[6];
            esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);

            hap_oven_initialize(ovenController, mac);
            ota.launchTask();
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
