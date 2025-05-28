/*#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "open62541.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_log.h"

static UA_Boolean running = true;

static void opcua_server_task(void *arg) {
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);

    UA_UInt16 port = 4840;
    UA_ServerConfig_setMinimalCustomBuffer(config, port, NULL, 16384, 16384); // send/recv Buffer
    // IP-Adresse ermitteln
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);

    char ip_str[16];
    esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));

    char endpointUrl[64];
    sprintf(endpointUrl, "opc.tcp://%s:%d", ip_str, port);

    ESP_LOGI("OPC UA", "Server läuft unter: %s", endpointUrl);

    UA_StatusCode status = UA_Server_run(server, &running);
    if (status != UA_STATUSCODE_GOOD) {
        ESP_LOGE("OPC UA", "Fehler beim Starten: 0x%" PRIx32, (uint32_t)status);
    }

    UA_Server_delete(server);
    printf("OPC UA Server beendet mit Statuscode: 0x%" PRIx32 "\n", (uint32_t)status);
    vTaskDelete(NULL);
}
*/
void app_main(void) {
    // (Hier ggf. WLAN-Init + IP-Wait)
    //xTaskCreate(&opcua_server_task, "opcua_server", 8192, NULL, 5, NULL);
}