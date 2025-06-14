#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "open62541.h"

static const char *TAG = "OPCUA";

void opcua_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starte OPC UA Server auf Port 4840");

    // Server anlegen
    UA_Server *server = UA_Server_new();
    if (!server) {
        ESP_LOGE(TAG, "Server konnte nicht erstellt werden");
        vTaskDelete(NULL);
        return;
    }

    // Konfiguration holen und minimal konfigurieren
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setMinimalCustomBuffer(config, 4840, NULL, 65536, 65536); 
    
    // Server starten (blockierend)
    UA_Boolean running = true;
    UA_StatusCode retval = UA_Server_run(server, &running);

    ESP_LOGI(TAG, "Server beendet mit Status: 0x%08lx", (unsigned long)retval);

    UA_Server_delete(server);
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Initialisiere OPC UA Task...");
    xTaskCreate(opcua_task, "opcua_task", 8192, NULL, 5, NULL);
}