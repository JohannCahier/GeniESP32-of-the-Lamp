/* Genie of the Lamp

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_event.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "driver/i2c.h"
#include "mdns.h"
#include "httpd.h"
#include "i2c_lamp_protocol.h"
#include "genius_connection.h"

#include "globalvars.h"
globals_t globals;

static const char *TAG = "genius-main";




static esp_err_t start_mdns_service()
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %d\n", err);
        return err;
    }

    //set hostname
    mdns_hostname_set(CONFIG_GENIUS_MDNS_HOSTNAME);
    //set default instance
    mdns_instance_name_set(CONFIG_GENIUS_MDNS_DESCRIPTION);

    // prolly not necessary, doesn't eat much bread anywayz...
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "MDNS Init done, local hostname is :");
    ESP_LOGW(TAG, "%s", CONFIG_GENIUS_MDNS_HOSTNAME);
    ESP_LOGI(TAG, "-----------------------------------\n\n");
    return ESP_OK;
}



extern const char *genie_ASCII_art[];
extern const unsigned int genie_ASCII_art_len;

void app_main(void)
{
    // ----------- On-boot greeter ------------------
    ESP_LOGI(TAG, "Genie of the Lamp sayz: << Hello! >> and wavez a hand\n");
    for (int i=0; i<genie_ASCII_art_len; i++)
        { ESP_LOGW(TAG, "%s", genie_ASCII_art[i]); }
    ESP_LOGI(TAG, ">>> Version: %d.%d \n", CONFIG_GENIUS_VERSION_MAJOR, CONFIG_GENIUS_VERSION_MINOR);
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

    ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    // ================================================

    globals.ambient_light = CONFIG_GENIUS_DEFAULT_AMBIENT_LIGHT;
    globals.state = ON;

    // Init/config i²c
    ESP_ERROR_CHECK(genius_i2c_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // start network and connect
    ESP_ERROR_CHECK(genius_connect());

    // to be reachable through mDNS...
    // << Yeah, moolticast, she knows it's a moolticast... >> Korben Dallas
    ESP_ERROR_CHECK(start_mdns_service());

    ESP_ERROR_CHECK(genius_httpd_start());

    // start sending i²c frames
    ESP_ERROR_CHECK(genius_i2c_enable(false));
}
