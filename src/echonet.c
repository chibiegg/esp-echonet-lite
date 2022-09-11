#include "echonet.h"
#include "echonet_internal.h"

TaskHandle_t echonetMainTaskHandle = NULL;

void echonet_main_task(void *pvParameters) {
    const TickType_t delay = 1000 / portTICK_PERIOD_MS;

    EchonetObjectHooks nodeProfileHooks = {0};
    nodeProfileHooks.getProperty = _el_node_profile_get_property;
    nodeProfileHooks.setProperty = _el_node_profile_set_property;

    uint8_t nodeProfileInfPropertyMap[] = {0x80, 0xD5, 0x00};
    uint8_t nodeProfileGetPropertyMap[] = {0x80, 0x82, 0x83, 0x8A, 0x8C, 0x9D, 0x9E, 0x9F, 0xD3, 0xD4, 0xD6, 0xD7, 0x00};

    EchonetObjectConfig nodeProfileObject = {0};
    nodeProfileObject.object = EOJNodeProfile;
    nodeProfileObject.instance = 1;
    nodeProfileObject.hooks = &nodeProfileHooks;
    nodeProfileObject.infPropertyMap = nodeProfileInfPropertyMap;
    nodeProfileObject.getPropertyMap = nodeProfileGetPropertyMap;
    

    EchonetConfig *cfg = _el_get_config();
    int sock = 0;
    while(1){
        if (sock != 0) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
        vTaskDelay(delay);

        // Create Multicast Socket
        sock = _el_create_multicast_ipv4_socket();
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create IPv4 multicast socket");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGD(TAG, "Create IPv4 multicast socket: %d", sock);
        cfg->sock = sock;

        int err = echonet_nodeprofile_send_inf();
        if (err < 0) {
            ESP_LOGE(TAG, "Send initialize INF error: %d", err);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        

        while (1) {
            char recvbuf[MAX_PACKET_SIZE];
            struct sockaddr_storage raddr; // Large enough for both IPv4 or IPv6

            int n = _el_receive_packet(sock, recvbuf, sizeof(recvbuf), &raddr);
            if (n < 0) {
                break;
            }
            if (n > 0) {
                EchonetPacket packet;
                int ret = _el_parse_packet((uint8_t *)recvbuf, n, &packet);
                if (ret < 0) {
                    ESP_LOGW(TAG, "Invalid Echonet Lite packet");
                } else {
                    ESP_LOGI(
                        TAG,
                        "TID: 0x%04x, SEOJ: 0x%04x %d, DEOJ: 0x%04x %d, ESV: 0x%02x, OPC: 0x%02x",
                        packet.transactionId,
                        packet.srcEchonetObject, packet.srcInstance,
                        packet.dstEchonetObject, packet.dstInstance,
                        packet.service, packet.operationCount
                    );

                    for (int i=0; i<packet.operationCount; i++) {
                        ESP_LOGI(TAG, "Operation %d EPC: 0x%02x PDC: 0x%02x", i, packet.operations[i].property, packet.operations[i].length);
                    }

                    ret = _el_check_waiting_packet(&packet);
                    if (ret != 0) {
                        // not waiting response packet
                        EchonetPacket responsePacket = {0};
                        uint8_t repsponseOperationsBuf[OPBUF_SIZE];
                        if (packet.dstEchonetObject == EOJNodeProfile && packet.dstInstance == 1) {
                            ESP_LOGD(TAG, "Using node profile");
                            if (_el_object_process_packet(&nodeProfileObject, &packet, &responsePacket, repsponseOperationsBuf) == 1) {
                                ESP_LOGI(TAG, "Response packet");
                                echonet_send_packet(&responsePacket, (struct sockaddr_in *)&raddr);
                            }
                        } else {
                            int matched = 0;
                            for (int i=0; i<cfg->objectCount; i++) {
                                if (cfg->objects[i].object == packet.dstEchonetObject && (packet.dstInstance == 0 || cfg->objects[i].instance == packet.dstInstance)) {
                                    matched++;
                                    ESP_LOGI(TAG, "Match object EOJ: 0x%04x%02x", cfg->objects[i].object, cfg->objects[i].instance);
                                    if (_el_object_process_packet(&(cfg->objects[i]), &packet, &responsePacket, repsponseOperationsBuf) == 1) {
                                        ESP_LOGI(TAG, "Response packet");
                                        echonet_send_packet(&responsePacket, (struct sockaddr_in *)&raddr);
                                    }
                                }
                            }
                            if (matched == 0) {
                                ESP_LOGW(TAG, "Invalid DEOJ 0x%04x 0x%02x", packet.dstEchonetObject, packet.dstInstance);
                            }
                        }
                    }
                }
            }
        }
    }
}


void echonet_start(EchonetConfig *config) {

    _el_set_config(config);
    ESP_LOGI(TAG, "Object count: %d",config->objectCount);

    xTaskCreatePinnedToCore(
        echonet_main_task,          // タスク関数
        "echonet",        // タスク名(あまり意味はない)
        16384,           // スタックサイズ
        NULL,           // 引数
        3,              // 優先度(大きい方が高い)
        &echonetMainTaskHandle,   // タスクハンドル
        APP_CPU_NUM     // 実行するCPU(PRO_CPU_NUM or APP_CPU_NUM)
    );
}
