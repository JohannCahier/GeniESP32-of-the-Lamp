/* This file is mostly copied from ESP-IDF Common functions for protocol examples, to establish Wi-Fi or Ethernet connection. NB Ethernet is not allowed as for now, but the ETH dedicated code has not been removed, allowing to re-enable the option

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "tcpip_adapter.h"

#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
#error "CONFIG_EXAMPLE_CONNECT_ETHERNET is not allowed as for now... And should be disabled in Kconfig file !!"
#define GENIUS_INTERFACE TCPIP_ADAPTER_IF_ETH
#endif

#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
#define GENIUS_INTERFACE TCPIP_ADAPTER_IF_STA
#endif

/**
 * @brief Configure Wi-Fi <strike>or Ethernet</strike>, connect, wait for IP
 *
 * This all-in-one helper function is used in protocols examples to
 * reduce the amount of boilerplate in the example.
 *
 * It is not intended to be used in real world applications.
 * See examples under examples/wifi/getting_started/ and examples/ethernet/
 * for more complete Wi-Fi or Ethernet initialization code.
 *
 * Read "Establishing Wi-Fi or Ethernet Connection" section in
 * examples/protocols/README.md for more information about this function.
 *
 * @return ESP_OK on successful connection
 */
esp_err_t genius_connect();

/**
 * Counterpart to example_connect, de-initializes Wi-Fi <strike>or Ethernet</strike>
 */
esp_err_t genius_disconnect();


#ifdef __cplusplus
}
#endif
