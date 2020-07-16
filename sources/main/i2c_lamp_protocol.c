#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_SLAVE_BUF_LEN 1024
#define I2C_PORT_MASTER 0
#define I2C_PORT_SLAVE 1


static const char *TAG="genius-i2c";

// i²c SLAVE task
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
        format_buf(data, size, str);
        ESP_LOGI(TAG, "Got i²2 data from Lamp : %s", str);
    }
}


// ======== GLOBALS =========

esp_err_t i2c_master_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = 18;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = 19;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(I2C_PORT_MASTER, &conf);
    return i2c_driver_install(I2C_PORT_MASTER, conf.mode, 0, 0, 0);
}

esp_err_t i2c_slave_init()
{
    esp_err_t err;
    i2c_config_t conf;
    conf.mode = I2C_MODE_SLAVE;
    conf.sda_io_num = 22;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = 23;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = 100000;
    conf.slave.addr_10bit_en = 0;
    conf.slave.slave_addr = CONFIG_GENIUS_I2C_SLAVE_ADDRESS;
    i2c_param_config(I2C_PORT_SLAVE, &conf);

    err= i2c_driver_install(I2C_PORT_SLAVE, conf.mode,
                            I2C_SLAVE_BUF_LEN,
                            I2C_SLAVE_BUF_LEN, 0);
    if (err == ESP_OK) {
        xTaskCreate(_i2c_slave_task, "i²c slave", 1024 * 2, (void *)0, 10, NULL);
    } else {
        ESP_LOGE(TAG, "Could not install i²c slave driver");
    }

    return err;
}

static esp_err_t i2c_master_write_slave(uint8_t destination, uint8_t *payload, uint8_t pl_size)
{
    assert(payload[0] == CONFIG_GENIUS_I2C_SLAVE_ADDRESS);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, destination << 1, true);
    i2c_master_write(cmd, payload, pl_size, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_MASTER, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static inline void
__attribute__((always_inline))
chksum(uint8_t dest, uint8_t *payload, uint8_t pl_size)
{
    dest <<= 1;
    for (uint8_t i=0; i<pl_size-1; i++) {
        dest ^= payload[i];
    }
    payload[pl_size-1] = dest;
}


#define SEND_FRAME(payload) \
        chksum(CONFIG_GENIUS_I2C_LAMP_SLAVE_ADDRESS, payload, sizeof(payload)); \
        return i2c_master_write_slave(CONFIG_GENIUS_I2C_LAMP_SLAVE_ADDRESS, payload, sizeof(payload));



esp_err_t send_i2c_heartbeat(uint8_t value) {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5  */
                         0x07, 0x06, 0x00, 0x00, 0xFF};

    payload[4] = value;

    SEND_FRAME(payload)
}

esp_err_t send_i2c_config() {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5    6      7     8     9  */
                        0x0B, 0x00, 0x02, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF};

    SEND_FRAME(payload)
}


esp_err_t send_i2c_ambiant_light(int16_t intensity) {
                      /*  0   */
    uint8_t payload[] = {CONFIG_GENIUS_I2C_SLAVE_ADDRESS,
                      /*  1     2     3     4     5     6  */
                        0x08, 0x05, 0x00, 0xab, 0xcd, 0xFF};

    payload[4] = intensity>>8;
    payload[5] = intensity & 0xFF;

    SEND_FRAME(payload)
}


