# ESP Echonet Lite Component

Framework for building Echonet Lite Node for ESP-IDF.

## Example

```C
#include "echonet.h"

uint8_t operation_status = 0;
uint8_t light_level = 0;

static int get_property(EchonetObjectConfig *object, EchonetOperation *ops) {
    switch (ops->property) {
        case EPCMonoFunctionalLightingOperationStatus:
            ESP_LOGI(TAG, "Instance %d, Get Operation Status", object->instance);
            ops->length = 1;
            ops->data[0] = operation_status;
            break;
        case EPCMonoFunctionalLightingLightLevel:
            ESP_LOGI(TAG, "Instance %d, Get Light Level", object->instance);
            ops->length = 1;
            ops->data[0] = light_level;
            break;
        default:
            return -1;
    }
    return 0;
}

static int set_property(EchonetObjectConfig *object, EchonetOperation *ops) {
    switch (ops->property) {
        case EPCMonoFunctionalLightingOperationStatus:
            ESP_LOGI(TAG, "Instance %d, Set Operation Status 0x%02x", object->instance, ops->data[0]);
            switch(ops->data[0]){
                case 0x30:
                case 0x31:
                    operation_status = ops->data[0];
                    break;
                default:
                    return -1;
            }
            break;
        case EPCMonoFunctionalLightingLightLevel:
            ESP_LOGI(TAG, "Instance %d, Set Light Level %d%%", object->instance, ops->data[0]);
            light_level = ops->data[0];
            break;
        default:
            return -1;
    }
    return 0;
}

int app_main() {
    EchonetConfig enconfig = {0};
    EchonetObjectHooks hooks = {0};
    hooks.getProperty = get_property;
    hooks.setProperty = set_property;

    uint8_t infPropertyMap[] = {
        EPCMonoFunctionalLightingOperationStatus,
        0x00
    };
    uint8_t getPropertyMap[] = {
        EPCMonoFunctionalLightingOperationStatus,
        EPCMonoFunctionalLightingLightLevel,
        ECHONET_LITE_DEFAULT_GET_PROPERTY_MAP,
        0x00
    };
    uint8_t setPropertyMap[] = {
        EPCMonoFunctionalLightingOperationStatus,
        0x00
    };

    EchonetObjectConfig objects[] = {
        {EOJMonoFunctionalLighting, 1, &hooks, infPropertyMap, getPropertyMap, setPropertyMap},
    };

    enconfig.objectCount = 1;
    enconfig.objects = objects;
    enconfig.Vendor = 0x00000B;
    enconfig.Serial = 0x0123456789abcdef;
    enconfig.Product = 0xfedcba9876543210;

    echonet_start(&enconfig);

    while(1) {
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
```

## API

```C
void echonet_start(EchonetConfig *config);
int echonet_send_packet(EchonetPacket *packet, struct sockaddr_in *addr);
int echonet_send_packet_and_wait_response(
    EchonetPacket *request, struct sockaddr_in *addr,
    EchonetPacket *response, uint8_t *buf
);
void echonet_prepare_packet(EchonetPacket *packet);
void echonet_prepare_response_packet(EchonetPacket *request, EchonetPacket *response);
void echonet_copy_packet(EchonetPacket *dst, uint8_t *buf, EchonetPacket *src);
int echonet_nodeprofile_send_inf();
```