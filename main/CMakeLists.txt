idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES open62541 esp_event esp_wifi esp_netif nvs_flash
    )
