#include <Arduino.h>
#include <M5Stack.h>
#include <WiFi.h>
#include "echonet.h"

// Configure Node Profile and Objects
EchonetConfig enconfig = {0};
EchonetObjectHooks hooks = {0};

uint8_t infPropertyMap[] = {EPCMonoFunctionalLightingOperationStatus, 0x00};
uint8_t getPropertyMap[] = {
  EPCMonoFunctionalLightingOperationStatus, EPCMonoFunctionalLightingLightLevel,
  0x81, 0x82, 0x83, 0x89, 0x8A, 0x8B,
  0x9d, 0x9e, 0x9f,
  0x00};
uint8_t setPropertyMap[] = {EPCMonoFunctionalLightingOperationStatus, EPCMonoFunctionalLightingLightLevel, 0x00};

EchonetObjectConfig objects[] = {
    {
      EOJMonoFunctionalLighting, 1, &hooks,
      {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber+1,
      infPropertyMap, getPropertyMap, setPropertyMap
    },
    {
      EOJMonoFunctionalLighting, 2, &hooks,
      {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber+2,
      infPropertyMap, getPropertyMap, setPropertyMap
    },
    {
      EOJMonoFunctionalLighting, 3, &hooks,
      {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber+3,
      infPropertyMap, getPropertyMap, setPropertyMap
    },
};



uint8_t operation_status_list[3] = {0x30, 0x30, 0x31};
uint8_t light_level_list[3] = {100, 100, 100};
uint8_t updated;

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
                updated = 1;
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
            updated = 1;
            break;
        default:
            ESP_LOGW(TAG, "Unsupported GET EPC 0x%02x", ops->property);
            return -1;
    }
    return 0;
}

void update_display(void) {
  // 文字サイズを変更
  M5.Lcd.setTextSize(2);

  for (uint8_t i=0; i<3; i++){
    uint32_t line_color = CYAN;
    uint32_t fill_color = CYAN;
    int x = 36;
    int y = 50 + 30 * i;

    char buf[32];
    if (operation_status_list[i] == 0x31) {
      line_color = CYAN;
      fill_color = BLACK;
      sprintf(buf, "%d  OFF", i, light_level_list[i]);
    } else {
      sprintf(buf, "%d %3d%%", i, light_level_list[i]);
    }
    int width = light_level_list[i] * 1.5;

    M5.Lcd.fillRect( x + 100, y, 100*1.5, 20, BLACK);
    M5.Lcd.fillRect( x + 100, y, width, 20, fill_color);
    M5.Lcd.drawRect( x + 100, y, width, 20, line_color);
    M5.Lcd.drawString(buf, x, y);
  }
}

void setup(void)
{
  M5.begin();
  Serial.begin(115200);

  M5.Lcd.println("connecting to WiFi");

  WiFi.begin("wifissid", "password");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }

  // Wi-Fi接続結果をシリアルモニタへ出力 
  Serial.println(""); 
  Serial.println("WiFi connected"); 
  Serial.println("IP address: "); 
  Serial.println(WiFi.localIP()); 

  // Configure Node Profile and Objects
  hooks.getProperty = get_property;
  hooks.setProperty = set_property;

  enconfig.manufacturer = 0xFFFF;
  enconfig.product = 0xfedcba9876543210;
  enconfig.serialNumber = 0x0123456789abcdef;
  enconfig.objectCount = 3;
  enconfig.objects = objects;

  // Start Echonet Lite Task in background
  echonet_start_and_wait(&enconfig, portMAX_DELAY);

  Serial.println("Echonet Lite Started");

  M5.Lcd.clear();
  updated = 1;
}

void loop(void)
{
  M5.update();
  // 表示更新
  if( updated ){
    updated = 0;
    update_display();
    delay(100);
  }
}
