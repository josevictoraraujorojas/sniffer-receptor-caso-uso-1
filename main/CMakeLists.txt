idf_component_register(SRCS "hello_world_main.c"
                       REQUIRES driver esp_wifi nvs_flash esp_netif mqtt
                       INCLUDE_DIRS "")