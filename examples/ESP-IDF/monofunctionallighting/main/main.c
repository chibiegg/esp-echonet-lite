#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi.h"
#include "echonet.h"

#define TAG "main"

uint8_t operation_status_list[3] = {0x31, 0x31, 0x31};
uint8_t light_level_list[3] = {100, 100, 100};

void send_inf(EchonetObjectConfig *object) {
  EchonetPacket packet;
  echonet_prepare_packet(&packet);
  packet.srcEchonetObject = object->object;
  packet.srcInstance = object->instance;
  packet.dstEchonetObject = EOJNodeProfile;
  packet.dstInstance = 1;
  packet.service = INF;
  packet.operationCount = 1;

  EchonetOperation ops;
  ops.property = EPCMonoFunctionalLightingOperationStatus;
  ops.length = 1;
  ops.data = &operation_status_list[object->instance-1];
  echonet_packet_add_operation(&packet, &ops);

  echonet_send_packet(&packet, NULL); // Multicast
}


static int get_property(EchonetObjectConfig *object, EchonetOperation *ops) {
    switch (ops->property) {
        case EPCMonoFunctionalLightingOperationStatus:
            printf("Instance %d, Get Operation Status 0x%02x\n", object->instance, operation_status_list[object->instance-1]);
            ops->length = 1;
            ops->data[0] = operation_status_list[object->instance-1];
            break;
        case EPCMonoFunctionalLightingLightLevel:
            printf("Instance %d, Get Light Level %d%%\n", object->instance, light_level_list[object->instance-1]);
            ops->length = 1;
            ops->data[0] = light_level_list[object->instance-1];
            break;
        default:
            return -1;
    }
    return 0;
}


static int set_property(EchonetObjectConfig *object, EchonetOperation *ops) {
    switch (ops->property) {
        case EPCMonoFunctionalLightingOperationStatus:
            printf("Instance %d, Set Operation Status 0x%02x\n", object->instance, ops->data[0]);
            switch (ops->data[0]) {
              case 0x30:
              case 0x31:
                operation_status_list[object->instance-1] = ops->data[0];
                send_inf(object);
                break;
              default:
                return -1;
            }
            break;
        case EPCMonoFunctionalLightingLightLevel:
            printf("Instance %d, Set Light Level %d%%\n", object->instance, ops->data[0]);
            if (ops->data[0] > 100) {
              return -1;
            }
            light_level_list[object->instance-1] = ops->data[0];
            break;
        default:
            ESP_LOGW(TAG, "Unsupported GET EPC 0x%02x", ops->property);
            return -1;
    }
    return 0;
}


void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Join to WiFi
    wifi_init_sta();


    // Configure Node Profile and Objects
    EchonetConfig enconfig = {0};
    EchonetObjectHooks hooks = {0};
    hooks.getProperty = get_property;
    hooks.setProperty = set_property;

    uint8_t infPropertyMap[] = {EPCMonoFunctionalLightingOperationStatus, 0x00};
    uint8_t getPropertyMap[] = {
      EPCMonoFunctionalLightingOperationStatus, EPCMonoFunctionalLightingLightLevel,
      0x81, 0x82, 0x83, 0x89, 0x8A, 0x8B,
      0x9d, 0x9e, 0x9f,
      0x00};
    uint8_t setPropertyMap[] = {EPCMonoFunctionalLightingOperationStatus, EPCMonoFunctionalLightingLightLevel, 0x00};

    enconfig.manufacturer = 0xFFFF;
    enconfig.product = 0xfedcba9876543210;
    enconfig.serialNumber = 0x0123456789abcdef;

    EchonetObjectConfig objects[] = {
        {
          EOJMonoFunctionalLighting, 1, &hooks,
          {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber,
          infPropertyMap, getPropertyMap, setPropertyMap
        },
        {
          EOJMonoFunctionalLighting, 2, &hooks,
          {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber,
          infPropertyMap, getPropertyMap, setPropertyMap
        },
        {
          EOJMonoFunctionalLighting, 3, &hooks,
          {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber,
          infPropertyMap, getPropertyMap, setPropertyMap
        },
    };

    enconfig.objectCount = 3;
    enconfig.objects = objects;
  
    // Start Echonet Lite Task in background
    echonet_start_and_wait(&enconfig, portMAX_DELAY);

    printf("Start Echonet Lite\n");

    vTaskDelay(1000/portTICK_PERIOD_MS);

    // Send INF each object.
    send_inf(&objects[0]);
    send_inf(&objects[1]);
    send_inf(&objects[2]);

    while(1) {
      // do nothing...
      vTaskDelay(portMAX_DELAY);
    }
}
