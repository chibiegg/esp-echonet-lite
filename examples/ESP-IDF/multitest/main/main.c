#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi.h"
#include "echonet.h"

#define TAG "main"

#define MAX_REGISTER_OBJECTS (50)

typedef struct {
    int idLength;
    uint8_t id[19];
    struct sockaddr_storage addr;
    uint16_t object;
    uint8_t instance;
} ObjectEntry;

ObjectEntry objectEntryList[MAX_REGISTER_OBJECTS] = {0};
ObjectEntry iterateObjectEntryList[MAX_REGISTER_OBJECTS] = {0};

bool equal_sockaddr_storage(struct sockaddr_storage *a, struct sockaddr_storage *b) {
    if (a->ss_family != b->ss_family || a->s2_len != b->s2_len) {
        return false;
    }
    switch(a->ss_family) {
        case AF_INET:
            {
                struct sockaddr_in *a4, *b4;
                a4 = (struct sockaddr_in *)a;
                b4 = (struct sockaddr_in *)b;
                if (a4->sin_addr.s_addr == b4->sin_addr.s_addr) {
                    return true;
                }
            }
            break;
        case AF_INET6:
            {
                struct sockaddr_in6 *a6, *b6;
                a6 = (struct sockaddr_in6 *)a;
                b6 = (struct sockaddr_in6 *)b;
                if (memcmp(&a6->sin6_addr, &b6->sin6_addr, sizeof(a6->sin6_addr)) == 0) {
                    return true;
                }
            }
            break;
    }
    return false;
}

ObjectEntry *add_object_entry(ObjectEntry *list, ObjectEntry *entry) {
    for (int i=0; i<MAX_REGISTER_OBJECTS;i++) {
        ObjectEntry *pos = &list[i];
        if (pos->idLength != 0 || pos->object != 0 || pos->instance != 0) {
            continue;
        }
        memcpy(pos, entry, sizeof(ObjectEntry));
        return pos;
    }
    return NULL;
}


ObjectEntry *get_object_entry_by_addr_eoj(ObjectEntry *list, struct sockaddr_storage *addr, uint16_t object, int8_t instance) {
    for (int i=0; i<MAX_REGISTER_OBJECTS;i++) {
        ObjectEntry *entry = &list[i];
        if (entry->idLength == 0) {
            continue;
        }
        if (!equal_sockaddr_storage(addr, &entry->addr)){
            continue;
        }
        if (entry->object != object || entry->instance != instance) {
            continue;
        }
        return entry;
    }
    return NULL;
}


int on_receive_inf(struct sockaddr_storage *addr, EchonetPacket *packet) {
    return 0;
}

void on_receive_node_instance_list_s(struct sockaddr_storage *addr, EchonetPacket *packet) {
    printf("on_receive_node_instance_list_s\r\n");

    if (packet->srcEchonetObject != EOJNodeProfile) {
        ESP_LOGW(TAG, "not EOJNodeProfile");
        return;
    }
    if (packet->service != Get_Res) {
        ESP_LOGW(TAG, "not Get_Res");
        return;
    }
    if (packet->operationCount != 1) {
        ESP_LOGW(TAG, "operationCount != 1");
        return;
    }
    EchonetOperation *ops = &packet->operations[0];
    if (ops->property != EPCNodeProfileSelfNodeInstanceListS) {
        ESP_LOGW(TAG, "not EPCNodeProfileSelfNodeInstanceListS");
        return;
    }

    uint8_t objectCount = ops->data[0];
    if (objectCount*3 + 1 != ops->length) {
        ESP_LOGW(TAG, "invalid length");
        return;
    }

    for (int i=0; i<objectCount; i++) {
        ObjectEntry entry = {0};
        entry.addr = *addr;
        entry.object = ((int16_t)ops->data[1 + 3*i + 0] << 8) | ops->data[1 + 3*i + 1];
        entry.instance = ops->data[1 + 3*i + 2];
        if (add_object_entry(iterateObjectEntryList, &entry) == NULL) {
            return;
        }
        ESP_LOGI(TAG, "EOJ: 0x%04x 0x%02x", entry.object, entry.instance);
    }
}

int update_object_info(ObjectEntry *entry) {
    EchonetPacket packet;
    uint8_t buf[CONFIG_EL_MAX_OPERATION_BUFFER_SIZE] = {0};
    echonet_prepare_packet(&packet);
    packet.srcEchonetObject = EOJNodeProfile;
    packet.srcInstance = 1;
    packet.dstEchonetObject = entry->object;
    packet.dstInstance = entry->instance;
    packet.service = Get;
    packet.operationCount = 1;
    packet.operations[0].property = EPCCommonId;
    if(echonet_send_packet_and_wait_response(&packet, (struct sockaddr_in *)&entry->addr, &packet, buf) < 0){
        ESP_LOGE(TAG, "get ID error");
        return -1;
    }

    if (packet.operationCount != 1) {
        ESP_LOGE(TAG, "operationCount != 1");
        return -1;
    }
    if (packet.service != Get_Res) {
        ESP_LOGI(TAG, "service != Get_Res");
        return -1;
    }
    if (packet.operationCount != 1) {
        ESP_LOGI(TAG, "operationCount != 1");
        return -1;
    }
    EchonetOperation *ops = &packet.operations[0];
    if (ops->property != EPCCommonId || ops->length < 1) {
        ESP_LOGI(TAG, "invalid length");
        return -1;
    }
    memcpy(entry->id, ops->data, ops->length);
    entry->idLength = ops->length;
    return 0;
}

void iterate_nodes() {
    memset(objectEntryList, 0x00, sizeof(objectEntryList));
    memset(iterateObjectEntryList, 0x00, sizeof(iterateObjectEntryList));

    // Get node InstanceListS of ALL nodes
    EchonetPacket packet;
    echonet_prepare_packet(&packet);
    packet.srcEchonetObject = EOJNodeProfile;
    packet.srcInstance = 1;
    packet.dstEchonetObject = EOJNodeProfile;
    packet.dstInstance = 1;
    packet.service = Get;
    packet.operationCount = 1;
    packet.operations[0].property = EPCNodeProfileSelfNodeInstanceListS;
    echonet_send_packet_and_wait_multiple_responses(&packet, NULL, on_receive_node_instance_list_s, 5000/portTICK_PERIOD_MS);

    // check all objects
    int count = 0;
    for (int i=0; i<MAX_REGISTER_OBJECTS; i++) {
        ObjectEntry *entry = &iterateObjectEntryList[i];
        if (entry->object != 0) {
            if(update_object_info(entry) < 0){
                // Error
                ESP_LOGE(TAG, "update_object_info error index:%d", i);
            }else{
                // OK
                if(add_object_entry(objectEntryList, entry) == NULL){
                    ESP_LOGE(TAG, "objectEntryList is full");
                }else{
                    ESP_LOGI(TAG, "Object registered");
                    count++;
                }
            }
        }
    }

    ESP_LOGI(TAG, "%d objects was detected", count);
}


void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Join to WiFi
    wifi_init_sta();

    // Configure Node Profile and Objects
    EchonetHooks hooks = {
        .onReceiveInf = on_receive_inf,
    };

    EchonetConfig enconfig = {0};
    enconfig.manufacturer = 0xFFFF;
    enconfig.product = 0xfedcba9876543210;
    enconfig.serialNumber = 0x0123456789abcdef;
    enconfig.objectCount = 0;
    enconfig.hooks = &hooks;

    // Start Echonet Lite Task in background
    echonet_start_and_wait(&enconfig, portMAX_DELAY);

    printf("Start Echonet Lite\n");

    while (1)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "starting iterate_nodes()");
        iterate_nodes();
    }
}
