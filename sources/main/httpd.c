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

static const char *TAG = "httpd";

static const char* state_str[] = {
    "ON",
    "OFF"
};

// #define MIN(x, y) ( ((x) < (y)) ? (x) ? (y) )

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

static esp_err_t state_post_handler(httpd_req_t *req) {
    char buffer[16];
    int ret;

    ESP_LOGI(TAG, "/state POST");

    if (req->content_len < sizeof(buffer)) {
        /* Read the data for the request */
        while ((ret = httpd_req_recv(req, buffer,
                        MIN(req->content_len, sizeof(buffer)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        for (int i=0; i<req->content_len; i++) {
            buffer[i] = tolower(buffer[i]);
        }

        // 0123456
        // state=
        if(strncmp("state=on", buffer, 8) == 0) {
            globals.state = ON;
        }
        else if(strncmp("state=off", buffer, 9) == 0) {
            globals.state = OFF;
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
    }

    // Send reply
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


static const httpd_uri_t state_post = {
    .uri       = "/state",
    .method    = HTTP_POST,
    .handler   = state_post_handler,
    .user_ctx  = NULL
};




static esp_err_t ambient_ligh_post_handler(httpd_req_t *req) {

    return ESP_OK;
}

static const httpd_uri_t ambient_light_post = {
    .uri       = "/ambient_light",
    .method    = HTTP_POST,
    .handler   = ambient_ligh_post_handler,
    .user_ctx  = NULL
};



void reboot(void) {
    ESP_LOGW(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
}


static esp_err_t reboot_post_handler(httpd_req_t *req) {
    static const char reboot_msg[] = "Rebooting now !";
    httpd_resp_send(req, reboot_msg, sizeof(reboot_msg));
    reboot();
    // reboot() wont return... But make the compiler happy:
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
