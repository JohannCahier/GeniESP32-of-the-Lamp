#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
// #include "nvs_flash.h"
#include <sys/param.h>
#include "tcpip_adapter.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include "mdns.h"

#include <esp_http_server.h>

static const char *TAG = "http";


static esp_err_t light_get_handler(httpd_req_t *req) {
    const char* on_off_str[] = {
        "ON",
        "OFF"
    };

    const char* reply = on_off_str[globals.state];
    httpd_resp_send(req, reply, strlen(reply));
}


static const httpd_uri_t lamp_get = {
    .uri       = "/light",
    .method    = HTTP_GET,
    .handler   = lamp_get_handler,
    .user_ctx  = NULL
};

static esp_err_t light_post_handler(httpd_req_t *req) {
    char buffer[16];
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "state", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => state=%s", param);
            }
        }
        free(buf);
    } else {
        // error
    }

    return ESP_OK;
}


static const httpd_uri_t lamp_post = {
    .uri       = "/lamp",
    .method    = HTTP_POST,
    .handler   = lamp_post_handler,
    .user_ctx  = NULL
};




static esp_err_t ambient_ligh_post_handler(httpd_req_t *req) {
    char buffer[16];
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "state", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => state=%s", param);
            }
        }
        free(buf);
    } else {
        // error
    }

    return ESP_OK;
}






static httpd_handle_t server = NULL;

void genius_httpd_init(void)
{

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    /* Start the server for the first time */
    server = start_webserver();
}
