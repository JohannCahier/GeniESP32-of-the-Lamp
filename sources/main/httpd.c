#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
// #include "nvs_flash.h"
#include <sys/param.h>
#include "tcpip_adapter.h"
#include "esp_eth.h"
#include <esp_http_server.h>

// #include <string.h>
#include "httpd.h"
#include "globalvars.h"
#include "i2c_lamp_protocol.h"

static const char *TAG = "httpd";

static const char* state_str[] = {
    "ON",
    "OFF"
};

// #define MIN(x, y) ( ((x) < (y)) ? (x) ? (y) )

static inline int
__attribute__((always_inline)) /*force inline whatever optimation level is enabled (see gcc -Ox)*/
find_char(char c, const char *str) {
    int i=0;
    while(str[i]) {
        if (str[i] == c)
            { return i; }
        i++;
    }
    return -1;
}


esp_err_t parse_ambient_light_value(const char* strval, int32_t *value) {
    int32_t value_tmp = 0;
    uint32_t weight = 1;
    int curr_digit_idx = strlen(strval); // after last digit

    ESP_LOGI(TAG, "Parse ambient light (string to parse : %s", strval);

    if (curr_digit_idx < 1) { // we want at least 1 digit
        ESP_LOGW(TAG, "Value error : no digit !");
        return ESP_FAIL;
    }
    while(curr_digit_idx) {
        char digit = strval[curr_digit_idx-1];

        if ((digit >= '0') && (digit <= '9')) {
            value_tmp += (digit-'0')*weight;
            weight *= 10;
            if (weight > 100000) {
                // number is too large !!
                // we expact signed 16bit value.
                ESP_LOGW(TAG, "Value too large dtected while parsing, value=%d, weight=%d, curr=%d", value_tmp, weight, curr_digit_idx);
                return ESP_FAIL;
            }
        } else if ((curr_digit_idx==1) && (digit == '-')) {
            value_tmp *= -1;
        } else {
            ESP_LOGW(TAG, "Parsing error, value=%d, weight=%d, curr=%d", value_tmp, weight, curr_digit_idx);
            return ESP_FAIL;
        }
        curr_digit_idx--;
    }
    if ((value_tmp < -32768) || (value_tmp >=32768)) {
        ESP_LOGW(TAG, "Parsing OK, but value=%d out-of-bounds [-32768..32767]", value_tmp);
        return ESP_FAIL;
    }
    *value = (int16_t)value_tmp;
    return ESP_OK;
}





static esp_err_t state_get_handler(httpd_req_t *req) {
    const char* reply = state_str[globals.state];
    ESP_LOGI(TAG, "/state GET");
    httpd_resp_send(req, reply, strlen(reply));
    return ESP_OK;
}


static const httpd_uri_t state_get = {
    .uri       = "/state",
    .method    = HTTP_GET,
    .handler   = state_get_handler,
    .user_ctx  = NULL
};





// TODO : get rid of those ugly bool * params... At least, use a structure embeding them
esp_err_t state_post_parse_param(char *param, bool *do_state, bool *do_light) {
    ESP_LOGI(TAG, "Parse param : '%s'", param);
    int sep_pos = find_char('=', param);

    if (sep_pos == -1) {
        ESP_LOGW(TAG, "No '=' sperator in parameter : %s",  param);
        return ESP_FAIL;
    }

    if        (strncmp(param, "state", sep_pos) == 0) {
        ESP_LOGI(TAG, "Parse state");
        if        (strncmp(param+sep_pos+1, "on", 2) == 0) {
            if (globals.state != ON) {
                globals.state = ON;
                *do_state = true;
            }
            return ESP_OK;
        } else if (strncmp(param+sep_pos+1, "off", 3) == 0) {
            if (globals.state != OFF) {
                globals.state = OFF;
                *do_state = true;
            }
            return ESP_OK;
        } else {
            ESP_LOGI(TAG, "Bad state '%s'", param+sep_pos+1);
            return ESP_FAIL;
        }
    } else if (strncmp(param, "ambient_light", sep_pos) == 0) {
        char *strval = param+sep_pos+1;
        int32_t value = 0;

        if (parse_ambient_light_value(strval, &value) != ESP_OK) {
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Parsing OK, set ambient light to %d", value);
        globals.ambient_light = value;
    } else if (strncmp(param, "turn_light_on", sep_pos) == 0) {
        if        (   (strncmp(param+sep_pos+1, "on", 2) == 0)
                   || (strncmp(param+sep_pos+1, "yes", 3) == 0)
                   || (strncmp(param+sep_pos+1, "true", 3) == 0)
                   || (strncmp(param+sep_pos+1, "1", 1) == 0)) {
            *do_light = true;
        }
    } else {
        ESP_LOGW(TAG, "Unknown parameter %s", param);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t state_post_handler(httpd_req_t *req) {
    const char* error_reason;
    char *buffer = malloc(req->content_len+1); // length + NUL TERMINATION
    int ret;

    ESP_LOGI(TAG, "/state POST");

    if (!buffer) {
        static char err_msg[] = "state_post_handler : could not allocate buffer !";
        ESP_LOGE(TAG, "%s", err_msg);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_msg);
    }

    while ((ret = httpd_req_recv(req, buffer, req->content_len)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            // Retry receiving if timeout occurred
            continue;
        }
        error_reason = "Error while receiving the request";
        goto fail;
    }

    // Don't care about case
    for (int i=0; i<req->content_len; i++) {
        buffer[i] = tolower(buffer[i]);
    }
    // NUL TERMINATION
    buffer[req->content_len] = 0;

    bool do_state = false, do_light = false;

    char *tok = strtok(buffer, "\r\n&");
    while(tok) {
        ret = state_post_parse_param(tok, &do_state, &do_light);
        if (ret != ESP_OK) {
            error_reason = tok;
            goto fail;
        }
        tok = strtok(NULL, "\r\n&");
    }

    if (do_state) {
        if (globals.state == ON) {
            ESP_LOGI(TAG, "STATE = ON !! (with%s light turned on)", do_light?"":"out");
            genius_i2c_enable(do_light);
        }
        if (globals.state == OFF) {
            ESP_LOGI(TAG, "STATE = OFF !!");
            genius_i2c_disable();
        }
    }


    /*
    if ( < sizeof(buffer)) {
        // Read the data for the request
        while ((ret = httpd_req_recv(req, buffer,
                        MIN(req->content_len, sizeof(buffer)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                // Retry receiving if timeout occurred
                continue;
            }
            return ESP_FAIL;
        }

        for (int i=0; i<req->content_len; i++) {
            buffer[i] = tolower(buffer[i]);
        }

        if(strncmp("state=on", buffer, 8) == 0) {
            globals.state = ON;
            ESP_LOGI(TAG, "STATE = ON !!");
        }
        else if(strncmp("state=off", buffer, 9) == 0) {
            globals.state = OFF;
            genius_i2c_disable();
            ESP_LOGI(TAG, "STATE = OFF !!");
        }
        else {
            ESP_LOGW(TAG, "Invalid state command : %s", buffer);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "  RECEIVED DATA : %.*s", ret, buffer);
    } else {
        ESP_LOGI(TAG, "String too long, rejecting if... (size == %d)", req->content_len);

        //HTTPD_400_BAD_REQUEST

        return ESP_FAIL;
    }*/

    // Send reply
    free(buffer);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;

fail:
    free(buffer);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error_reason);
    return ret;
}

static const httpd_uri_t state_post = {
    .uri       = "/state",
    .method    = HTTP_POST,
    .handler   = state_post_handler,
    .user_ctx  = NULL
};





static esp_err_t ambient_light_post_handler(httpd_req_t *req) {
    const char* error_reason;
    char *buffer = malloc(req->content_len+1); // length + NUL TERMINATION
    int ret;

    ESP_LOGI(TAG, "/ambient_light POST");

    if (!buffer) {
        static char err_msg[] = "state_post_handler : could not allocate buffer !";
        ESP_LOGE(TAG, "%s", err_msg);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_msg);
    }

    while ((ret = httpd_req_recv(req, buffer, req->content_len)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            // Retry receiving if timeout occurred
            continue;
        }
        error_reason = "Error while receiving the request";
        goto fail;
    }

    // Don't care about case
    for (int i=0; i<req->content_len; i++) {
        buffer[i] = tolower(buffer[i]);
    }
    // NUL TERMINATION
    buffer[req->content_len] = 0;

    if (strncmp(buffer, "value", 5) == 0) {
        char *strval = buffer+6;
        int32_t value = 0;

        if (parse_ambient_light_value(strval, &value) != ESP_OK) {
            error_reason = "Error while parsing value";
            goto fail;
        }
        ESP_LOGI(TAG, "Parsing OK, set ambient light to %d", value);
        globals.ambient_light = value;
    } else {
        ESP_LOGW(TAG, "Unknown parameter %s", buffer);
        error_reason = "Unknown parameter";
        goto fail;
    }

    free(buffer);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;

fail:
    free(buffer);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error_reason);
    return ret;
}

static const httpd_uri_t ambient_light_post = {
    .uri       = "/ambient_light",
    .method    = HTTP_POST,
    .handler   = ambient_light_post_handler,
    .user_ctx  = NULL
};





void reboot(void* _) {
    vTaskDelay(CONFIG_GENIUS_REBOOT_TASK_DELAY_MS / portTICK_RATE_MS);
    ESP_LOGW(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

static esp_err_t reboot_post_handler(httpd_req_t *req) {
    static const char reboot_msg[] = "Rebooting now !";
    httpd_resp_send(req, reboot_msg, sizeof(reboot_msg));
    xTaskCreate(reboot, "reboot", 1024 * 2, (void *)0, 10, NULL);
    return ESP_OK;
}

static const httpd_uri_t reboot_post = {
    .uri       = "/reboot",
    .method    = HTTP_POST,
    .handler   = reboot_post_handler,
    .user_ctx  = NULL
};





static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &state_get);
        httpd_register_uri_handler(server, &state_post);
        httpd_register_uri_handler(server, &ambient_light_post);
        httpd_register_uri_handler(server, &reboot_post);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}



static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


#define RETURN_ON_ERROR(x) {    \
    esp_err_t err = (x);        \
    if (err != ESP_OK) {        \
        return err;             \
    }                           \
}


esp_err_t genius_httpd_start(void)
{
    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_GENIUS_CONNECT_WIFI
    RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &globals.http_server));
    RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &globals.http_server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_GENIUS_CONNECT_ETHERNET
    RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &globals.http_server));
    RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &globals.http_server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    /* Start the server for the first time */
    globals.http_server = start_webserver();

    return ESP_OK;
}
