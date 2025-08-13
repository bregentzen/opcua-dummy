#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"

#include "open62541.h"

#define TAG "OPCUA"

// --- Tunables ---------------------------------------------------------------
#define OPCUA_TASK_STACK_SIZE  (32 * 1024)   // open62541 needs room; 16 KiB was tight
#define OPCUA_TASK_PRIORITY    5
#define OPCUA_TCP_PORT         4840

// --- Wi‑Fi connection handling ---------------------------------------------
static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi‑Fi disconnected; retrying…");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_and_connect(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Buschfunk Bregen",
            .password = "Gyrossuppe01",
            // Rely on defaults for WPA2/3; IDF negotiates SAE automatically if AP supports it.
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Warte auf WLAN‑Verbindung…");
    // Wait up to 15s for IP
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    if ((bits & WIFI_CONNECTED_BIT) == 0) {
        ESP_LOGE(TAG, "No IP within timeout; continuing but OPC UA will not start.");
    }
}

// --- OPC UA server task -----------------------------------------------------
static void opcua_server_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starte OPC UA Task…");

    // IMPORTANT: Ensure your open62541 is built with UA_ENABLE_TCP=ON.

    // Create config with proper network layer pre‑initialized
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);

    // Set TCP port using network layer configuration
    UA_ServerConfig_setMinimal(config, OPCUA_TCP_PORT, NULL);

    // Anonymous access control for old API
    UA_AccessControl_default(config,
        true,  // allowAnonymous
        NULL,  // array of username/password
        0,     // array length
        NULL   // user login callback
    );


    server = UA_Server_new(); // takes ownership of config
    if(!server) {
        ESP_LOGE(TAG, "UA_Server_new() failed");
        UA_ServerConfig_clean(config);
        vTaskDelete(NULL);
        return;
    }

    // Example node: Int32 variable under Objects
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        UA_Int32 myInteger = 42;
        UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "MyInteger");

        UA_NodeId nodeId   = UA_NODEID_STRING(1, "myInteger");
        UA_QualifiedName bn= UA_QUALIFIEDNAME(1, "MyInteger");
        UA_StatusCode st = UA_Server_addVariableNode(
            server,
            nodeId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            bn,
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr,
            NULL,
            NULL);
        if(st != UA_STATUSCODE_GOOD) {
            ESP_LOGE(TAG, "Failed to add variable: 0x%08x", (unsigned)st);
        }
    }

    UA_StatusCode rc = UA_Server_run_startup(server);
    if(rc != UA_STATUSCODE_GOOD) {
        ESP_LOGE(TAG, "Startup error: 0x%08x", (unsigned)rc);
        UA_Server_delete(server);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "OPC UA listening at opc.tcp://<esp32-ip>:%u", (unsigned)OPCUA_TCP_PORT);

    // Non‑blocking iterate loop
    for(;;) {
        UA_Server_run_iterate(server, false);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Not reached in this example, but here for completeness
    UA_Server_run_shutdown(server);
    UA_Server_delete(server);
    vTaskDelete(NULL);
}

void app_main(void) {
    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Wi‑Fi + IP
    wifi_init_and_connect();

    // Only start OPC UA if we have an IP address now
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    if (bits & WIFI_CONNECTED_BIT) {
        // Pin to core 0 to avoid contention with Wi‑Fi (which runs on core 0 by default as well).
        // If you see starvation, move to core 1. Keep priority moderate.
        xTaskCreatePinnedToCore(opcua_server_task, "opcua_task",
                                OPCUA_TASK_STACK_SIZE, NULL,
                                OPCUA_TASK_PRIORITY, NULL, 1);
    } else {
        ESP_LOGE(TAG, "Skipping OPC UA start because no IP was obtained");
    }

    // Idle loop
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
