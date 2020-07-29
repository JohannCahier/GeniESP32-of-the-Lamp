#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "globalvars.h"

#define I2C_SLAVE_BUF_LEN 1024
#define I2C_PORT_MASTER 0
#define I2C_PORT_SLAVE 1


static const char *TAG="genius-i2c";

// =========== i²c SLAVE task/routines ====================
static void format_buf(uint8_t *buf, int len, char *str)
{
    int i;
    for (i = 0; i < len; i++) {
        sprintf(str, "%02x ", buf[i]);
        str += 3;
    }
}

static void _i2c_slave_task(void* _) {
    uint8_t data[16] = {0};
    char str[64];
    while(1) {
        int size = i2c_slave_read_buffer(I2C_PORT_SLAVE, data, 16, 1000 / portTICK_RATE_MS);
        if (size > 0) {
            format_buf(data, size, str);
            ESP_LOGI(TAG, "Got i²2 data from Lamp : %s", str);
        }
    }
}



// =========== i²c master tasks/routines===================

static inline void
__attribute__((always_inline)) /*force inline whatever optimation level is enabled (see gcc -Ox)*/
chksum(uint8_t dest, uint8_t *payload, uint8_t pl_size)
{
    dest <<= 1;
    for (uint8_t i=0; i<pl_size-1; i++) {
        dest ^= payload[i];
    }
    payload[pl_size-1] = dest;
}


static esp_err_t i2c_master_write_slave(uint8_t destination, uint8_t *payload, uint8_t pl_size)
{
    esp_err_t err;
//     static unsigned long long last_frame_ts = 0;
//
//     while (last_frame_ts != 0 && esp_timer_get_time()-last_frame_ts > 20000) {
//         vTaskDelay(20/portTICK_RATE_MS);
//     }

    assert(payload[0] == CONFIG_GENIUS_I2C_SLAVE_ADDRESS);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, destination << 1, true);
    i2c_master_write(cmd, payload, pl_size, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_PORT_MASTER, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

//     last_frame_ts = esp_timer_get_time();

    return err;
}

#define SEND_FRAME(payload) \
        chksum(CONFIG_GENIUS_I2C_LAMP_SLAVE_ADDRESS, payload, sizeof(payload)); \
        return i2c_master_write_slave(CONFIG_GENIUS_I2C_LAMP_SLAVE_ADDRESS, payload, sizeof(payload));




static esp_err_t genius_i2c_send_heartbeat(uint8_t value) {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5  */
                         0x07, 0x06, 0x00, 0x00, 0xFF};

    payload[4] = value;

    SEND_FRAME(payload);
}

esp_err_t genius_i2c_send_config() {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5    6      7     8     9  */
                        0x0B, 0x00, 0x02, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF};

    SEND_FRAME(payload);
}


esp_err_t genius_i2c_send_ambiant_light(int16_t intensity) {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5     6  */
                        0x08, 0x05, 0x00, 0xab, 0xcd, 0xFF};

    payload[4] = intensity>>8;
    payload[5] = intensity & 0xFF;

    SEND_FRAME(payload);
}


esp_err_t genius_i2c_send_button_down() {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5     6  */
                        0x08, 0x07, 0x00, 0x01, 0x00, 0xFF};

    SEND_FRAME(payload);
}

// tempo : duration of the button press, in 0.1 seconds (max 25.5 s)
esp_err_t genius_i2c_send_button_up(uint8_t tempo) {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5     6  */
                        0x08, 0x07, 0x00, 0x00, tempo, 0xFF};

    SEND_FRAME(payload);
}

// ----- tasks -----

#define PERIOD_HEARTBEAT        CONFIG_GENIUS_I2C_HEARTBEAT_PERIOD
#define DELAY_HEARTBEAT         CONFIG_GENIUS_I2C_HEARTBEAT_DELAY
//#define PERIOD_CONFIG           CONFIG_GENIUS_I2C_CONFIG_PERIOD
//#define PERIOD_AMBIENT_LIGHT    CONFIG_GENIUS_I2C_AMBIENT_LIGHT_PERIOD

void heartbeat_once(void* arg) {
    if (arg) ESP_LOGW(TAG, "<3");

    while (genius_i2c_send_heartbeat(0) != ESP_OK) {
        ESP_LOGW(TAG, "ERR : heartbeat(0)");
    }
    vTaskDelay(DELAY_HEARTBEAT/ portTICK_RATE_MS);
    while (genius_i2c_send_heartbeat(1) != ESP_OK) {
        ESP_LOGW(TAG, "ERR : heartbeat(1)");
    }
}

void task_heartbeat(void *arg) {
    while(1) {
        heartbeat_once(arg);
        vTaskDelay( (PERIOD_HEARTBEAT-DELAY_HEARTBEAT) / portTICK_RATE_MS);
    }
}
/*
void task_config(void *arg) {
    while(1) {
        while (genius_i2c_send_config() != ESP_OK) {
            ESP_LOGW(TAG, "ERR : config");
        }
        vTaskDelay(PERIOD_CONFIG / portTICK_RATE_MS);
    }
}

void task_ambiant_light(void *arg) {
    while(1) {
        if (genius_i2c_send_ambiant_light(globals.ambient_light) != ESP_OK) {
            ESP_LOGW(TAG, "ERR : ambiance()");
        }
        vTaskDelay(PERIOD_AMBIENT_LIGHT / portTICK_RATE_MS);
    }
}

void cb_initial_click(void *arg) {
    ESP_LOGI(TAG, "timer : clic !!");
    while (genius_i2c_send_button_down() != ESP_OK) {
        ESP_LOGW(TAG, "timer : retry btn down");
    }
    vTaskDelay(100 / portTICK_RATE_MS);
    while (genius_i2c_send_button_up(1) != ESP_OK) {
        ESP_LOGW(TAG, "timer : retry btn up");
    }
}*/

//  =========== API IMPLEMENTATION ========================

static i2c_config_t conf_master;
static i2c_config_t conf_slave;

static TaskHandle_t task_config_hdl = NULL;
static TaskHandle_t task_heartbeat_hdl = NULL;
static TaskHandle_t task_ambient_light_hdl = NULL;
static TaskHandle_t task_slave_hdl = NULL;
static TimerHandle_t iniclk = NULL;

esp_err_t genius_i2c_init()
{

/*
    iniclk = xTimerCreate("Timer",
                          CONFIG_GENIUS_OFF_CLICK_DELAY_MS / portTICK_RATE_MS,
                          pdFALSE, // no auto-reload (ONESHOT)
                          NULL,  // don't use ID
                          cb_initial_click);*/

    // config for slave
    conf_slave.mode = I2C_MODE_SLAVE;
    conf_slave.sda_io_num = 22;
    conf_slave.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf_slave.scl_io_num = 23;
    conf_slave.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf_slave.master.clk_speed = 100000;
    conf_slave.slave.addr_10bit_en = 0;
    conf_slave.slave.slave_addr = CONFIG_GENIUS_I2C_SLAVE_ADDRESS;
    i2c_param_config(I2C_PORT_SLAVE, &conf_slave);

    // config for master
    conf_master.mode = I2C_MODE_MASTER;
    conf_master.sda_io_num = 18;
    conf_master.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf_master.scl_io_num = 19;
    conf_master.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf_master.master.clk_speed = 100000;
    i2c_param_config(I2C_PORT_MASTER, &conf_master);

    // YOIMIDO : You Only Install Master I²c Driver Once
    return i2c_driver_install(I2C_PORT_MASTER, conf_master.mode, 0, 0, 0);
}




// NB : to reduce i²c traffic jam, because the local master is unable to keep quiet when another master talks to the local slave, and creates congestion (it seems)
// So create the only "pinging" task for heartbeat, when wanted to light up the lamp when setting up i²c comm.
// NOTE : sending heartbeat ONLY once never ligthed up the lamp... But maybe sending it *INDEFINITELY* is overkill... Maybe try sending it twice, or even three times...

esp_err_t genius_i2c_enable(bool with_heartbeat) {
    // seems needed (?)
    i2c_param_config(I2C_PORT_SLAVE, &conf_slave);
    // install slave driver
    esp_err_t err= i2c_driver_install(I2C_PORT_SLAVE, conf_slave.mode,
                                      I2C_SLAVE_BUF_LEN,
                                      I2C_SLAVE_BUF_LEN, 0);
    if (err == ESP_OK) {
        xTaskCreate(_i2c_slave_task, "i²c slave", 1024 * 2, (void *)0, 10, &task_slave_hdl);

        while (genius_i2c_send_config() != ESP_OK) {
            ESP_LOGW(TAG, "ERR : config");
        }

        // start tasks to "ping" the Lamp
        if (with_heartbeat)
            xTaskCreate(task_heartbeat, "heartbeat", 1024 * 2, (void *)true, 10, &task_heartbeat_hdl);

        vTaskDelay(500 / portTICK_RATE_MS);

        while (genius_i2c_send_ambiant_light(globals.ambient_light) != ESP_OK) {
            ESP_LOGW(TAG, "ERR : ambiance()");
        }
    } else {
        ESP_LOGE(TAG, "Could not install i²c slave driver");
    }

    return err;
}


esp_err_t genius_i2c_disable() {
    // stop pinging tasks
    if (task_config_hdl) {
        vTaskDelete(task_config_hdl);
        task_config_hdl = NULL;
    }
    if (task_heartbeat_hdl) {
        vTaskDelete(task_heartbeat_hdl);
        task_heartbeat_hdl = NULL;
    }
    if (task_ambient_light_hdl) {
        vTaskDelete(task_ambient_light_hdl);
        task_ambient_light_hdl = NULL;
    }

    if (task_slave_hdl) {
        vTaskDelete(task_slave_hdl);
        task_slave_hdl = NULL;
    }

    // uninstall slave driver
    i2c_driver_delete(I2C_PORT_SLAVE);

    return ESP_OK;
}



