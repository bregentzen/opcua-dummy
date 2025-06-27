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
#include "esp_sntp.h"

#include "open62541.h"

#define TAG "OPCUA"
#define OPCUA_TASK_STACK_SIZE 131072  // 16 KB Stack für OPC UA Task
#define OPCUA_TASK_PRIORITY   5      // Priorität im normalen Bereich

static void opcua_server_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starte OPC UA Task…");

    UA_Server *server = UA_Server_new();
    if(!server) {
        ESP_LOGE(TAG, "Fehler beim Erstellen des OPC UA Servers");
        vTaskDelete(NULL);
        return;
    }

    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);

    // Beispiel: Füge eine Variable im Address Space hinzu
    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "myInteger");
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Int32 myInteger = 42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "MyInteger");

    UA_StatusCode addStatus = UA_Server_addVariableNode(
        server,
        myIntegerNodeId,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), // Parent Node: Objects folder
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),     // ReferenceType
        UA_QUALIFIEDNAME(1, "MyInteger"),             // BrowseName
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr,
        NULL,
        NULL);

    if(addStatus != UA_STATUSCODE_GOOD) {
        ESP_LOGE(TAG, "Variable konnte nicht hinzugefügt werden: 0x%08x", (unsigned int)addStatus);
    }

    UA_StatusCode rc = UA_Server_run_startup(server);
    if(rc != UA_STATUSCODE_GOOD) {
        ESP_LOGE(TAG, "Startup-Fehler: 0x%08x", (unsigned int)rc);
        UA_Server_delete(server);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "OPC UA Server läuft auf opc.tcp://<esp32-ip>:4840");

    while(true) {
        // True = blockierend, besser bei FreeRTOS aber nicht für längere Blocking calls
        // Besser: false + kurze Delays (oder kein Delay)
        UA_Server_run_iterate(server, false);
        vTaskDelay(pdMS_TO_TICKS(10)); // 10 ms Delay zum Yield
    }

    UA_Server_run_shutdown(server);
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