idf_component_register(SRCS "udp_receiver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip f1_telemetry)