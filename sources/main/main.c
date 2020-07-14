/* Genie of the Lamp

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/i2c.h"
#include "httpd.h"
#include "globalvars.h"

inline void chksum(uint8_t dest, uint8_t *payload, uint8_t pl_size) __attribute__((always_inline));

// USER MUST Specify destination address as the 7-bit (bits 0..6) i2c slave addr

static const i2c_port_t i2c_port_master = I2C_NUM_0;
static const i2c_port_t i2c_port_slave = I2C_NUM_1;
static const uint8_t i2c_own_address = 0x20;
static const uint8_t i2c_lamp_address = 0x08;

#define I2C_SLAVE_TX_BUF_LEN 1024
#define I2C_SLAVE_RX_BUF_LEN 1024
#define ESP_SLAVE_ADDR i2c_own_address
#define PERIOD_HEARTBEAT 1000   // 1s
#define DELAY_HEARTBEAT 200     // 0.2s (between HB(0) and HB(1)
#define PERIOD_AMBIANCE 1250     // 1.25s
#define PERIOD_CONFIG 10000     // 10s

// ======== GLOBALS =========

static esp_err_t i2c_master_init(i2c_port_t i2c_port)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = 18;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = 19;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(i2c_port, &conf);
    return i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
}

static esp_err_t i2c_slave_init(i2c_port_t i2c_port)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_SLAVE;
    conf.sda_io_num = 22;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = 23;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = 100000;
    conf.slave.addr_10bit_en = 0;
    conf.slave.slave_addr = ESP_SLAVE_ADDR;
    i2c_param_config(i2c_port, &conf);
    return i2c_driver_install(i2c_port, conf.mode,
                              I2C_SLAVE_RX_BUF_LEN,
                              I2C_SLAVE_RX_BUF_LEN, 0);
}


static esp_err_t i2c_master_write_slave(uint8_t destination, uint8_t *payload, uint8_t pl_size)
{
    assert(payload[0] == i2c_own_address);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, destination << 1, true);
    i2c_master_write(cmd, payload, pl_size, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port_master, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void chksum(uint8_t dest, uint8_t *payload, uint8_t pl_size) {
    dest <<= 1;
    for (uint8_t i=0; i<pl_size-1; i++) {
        dest ^= payload[i];
    }
    payload[pl_size-1] = dest;
}

esp_err_t heartbeat(uint8_t value) {
                      /*  0     1     2     3     4     5  */
    uint8_t payload[] = {0x20, 0x07, 0x06, 0x00, 0x00, 0xFF};
    const uint8_t payload_size = 6;

    payload[4] = value;
    chksum(i2c_lamp_address, payload, payload_size);
    return i2c_master_write_slave(i2c_lamp_address, payload, payload_size);
}

esp_err_t send_config() {
                      /*  0     1     2     3     4     5    6      7     8     9  */
    uint8_t payload[] = {0x20, 0x0B, 0x00, 0x02, 0x04, 0x05, 0x06, 0x07, 0x08, 0x31};
    const uint8_t payload_size = 10;
    // no variable part, cheksum already done
    return i2c_master_write_slave(i2c_lamp_address, payload, payload_size);
}


esp_err_t send_ambiance(uint16_t intensity) {
                      /*  0     1     2     3     4     5     6  */
    uint8_t payload[] = {0x20, 0x08, 0x05, 0x00, 0xab, 0xcd, 0xFF};
    const uint8_t payload_size = 7;

    payload[4] = intensity>>8;
    payload[5] = intensity & 0xFF;

    chksum(i2c_lamp_address, payload, payload_size);
    return i2c_master_write_slave(i2c_lamp_address, payload, payload_size);
}


void reboot() {
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

void task_heartbeat(void *arg) {
    while(1) {
        if (heartbeat(0) != ESP_OK) {
            printf("ERR : heartbeat(0)\n");
        }
        vTaskDelay(DELAY_HEARTBEAT/ portTICK_RATE_MS);
        if (heartbeat(1) != ESP_OK) {
            printf("ERR : heartbeat(1)\n");
        }
        vTaskDelay( (PERIOD_HEARTBEAT-DELAY_HEARTBEAT) / portTICK_RATE_MS);
    }
}

void task_config(void *arg) {
    while(1) {
        if (send_config() != ESP_OK) {
            printf("ERR : config\n");
        }
        vTaskDelay(PERIOD_CONFIG / portTICK_RATE_MS);
    }
}

void task_ambiance(void *arg) {
    while(1) {
        if (send_ambiance(0x200) != ESP_OK) {
            printf("ERR : ambiance()\n");
        }
        vTaskDelay(PERIOD_AMBIANCE / portTICK_RATE_MS);
    }
}

static void disp_buf(uint8_t *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}


void start_mdns_service()
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    //set hostname
    mdns_hostname_set(CONFIG_GENIUS_MDNS_HOSTNAME);
    //set default instance
    mdns_instance_name_set(CONFIG_GENIUS_MDNS_DESCRIPTION);

    // prolly not necessary, doesn't eat much bread anywayz...
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    printf("MDNS Init done...\n");
}


}


void app_main(void)
{
    // ----------- On-boot greeter ------------------
    printf("Genie of the Lamp sayz: << Hello! >> and wavez a hand o/\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    fflush(stdout);
    // ================================================


    // Start i²c
    ESP_ERROR_CHECK(i2c_master_init(i2c_port_master));
    ESP_ERROR_CHECK(i2c_slave_init(i2c_port_slave));

    // start network and connect
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(genius_connect());

    /* to be reachable...*/
    ESP_ERROR_CHECK(start_mdns_service());



    xTaskCreate(task_config, "config", 1024 * 2, (void *)0, 10, NULL);
    xTaskCreate(task_heartbeat, "heartbeat", 1024 * 2, (void *)1, 10, NULL);
    xTaskCreate(task_ambiance, "ambiance", 1024 * 2, (void *)2, 10, NULL);

    uint8_t *data = (uint8_t *)malloc(16);
    while(1) {
        int size = i2c_slave_read_buffer(i2c_port_slave, data, 16, 1000 / portTICK_RATE_MS);
        printf("------------------\n");
        disp_buf(data, size);
        printf("==================\n");
    }
    // should never reach this point !
    reboot();
}
