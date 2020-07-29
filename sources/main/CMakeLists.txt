idf_component_register(SRCS "main.c"
                       "i2c_lamp_protocol.c"
                       "genius_connection.c"
                       "httpd.c"
                       "genie_ascii_art.c"
                       "slave_adressed_to_gpio.c"
                       "simple_ota_update.c"
                    EMBED_TXTFILES "certs/johann_laptop_fw_update.pem"
                    INCLUDE_DIRS "")
