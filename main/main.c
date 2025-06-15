#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "open62541.h"

#define TAG "OPCUA"
#define OPCUA_TASK_STACK_SIZE 16384  // 16 KB Stack für OPC UA Task
#define OPCUA_TASK_PRIORITY   5      // Priorität im normalen Bereich

void opcua_server_task(void *pvParameters) {
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);

    // Für Minimalbetrieb ohne Security
    UA_ServerConfig_setMinimal(config, 4840, NULL);

    ESP_LOGI(TAG, "Starte OPC UA Server...");

    UA_Boolean running = true;
    UA_StatusCode retval = UA_Server_run(server, &running);
    if(retval != UA_STATUSCODE_GOOD) {
        ESP_LOGE(TAG, "Fehler beim Starten: 0x%08x", (unsigned int)retval);
    }

    while (running) {
        UA_Server_run_iterate(server, true);  // true = blockierend
        ESP_LOGI(TAG, "Server läuft noch...");
        vTaskDelay(pdMS_TO_TICKS(1000));      // Verhindert Watchdog-Crash
    }
    UA_Server_delete(server);
    vTaskDelete(NULL);
}


void app_main(void) {
    // NVS initialisieren
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Netzwerk initialisieren
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Buschfunk Bregen",
            .password = "Gyrossuppe01",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Warte auf WLAN-Verbindung...");
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Warte auf IP-Adresse
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Starte den OPC UA Server in einer separaten Task
    xTaskCreate(opcua_server_task, "opcua_task", OPCUA_TASK_STACK_SIZE, NULL,
                OPCUA_TASK_PRIORITY, NULL);
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Idle Loop, verhindert Return
    }
}