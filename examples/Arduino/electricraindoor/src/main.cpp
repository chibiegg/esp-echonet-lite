#include <Arduino.h>
#include <M5Stack.h>
#include <WiFi.h>
#include "echonet.h"
#include "esp_log.h"

typedef enum {
  OperationClose = 0x42,
  OperationOpen = 0x41,
  OperationStop = 0x43,
} Operation;

typedef enum {
  StateFullyOpen = 0x41,
  StateFullyClosed = 0x42,
  StateOpening = 0x43,
  StateClosing = 0x44,
  StateStoppedHalfway = 0x45,
} State;


// Configure Node Profile and Objects
EchonetConfig enconfig = {
  .manufacturer = 0x00B6,
  .product = 0xfedcba9876543210,
  .serialNumber = 0x0123456789abcdef,
};
EchonetObjectHooks hooks = {0};

uint8_t infPropertyMap[] = {
  EPCCommonOperationStatus, EPCElectricRainDoorOpenCloseStatus, 0x00
};
uint8_t getPropertyMap[] = {
  EPCCommonOperationStatus,
  EPCElectricRainDoorOpenCloseOperation, EPCElectricRainDoorOpenCloseStatus,
  ECHONET_LITE_DEFAULT_GET_PROPERTY_MAP,
  0x00
};
uint8_t setPropertyMap[] = {
  EPCCommonOperationStatus, EPCElectricRainDoorOpenCloseOperation, 0x00
};

EchonetObjectConfig objects[] = {
    {
      EOJElectricRainDoor, 1, &hooks,
      {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber+1,
      infPropertyMap, getPropertyMap, setPropertyMap
    },
    {
      EOJElectricRainDoor, 2, &hooks,
      {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber+2,
      infPropertyMap, getPropertyMap, setPropertyMap
    },
    {
      EOJElectricRainDoor, 1, &hooks,
      {'M', 0x00}, enconfig.manufacturer, enconfig.product, enconfig.serialNumber+3,
      infPropertyMap, getPropertyMap, setPropertyMap
    },
};


uint8_t status_list[3] =  {StateFullyOpen, StateFullyOpen, StateFullyOpen};
uint8_t operation_list[3] = {OperationStop, OperationStop, OperationStop};
uint8_t open_level[3] = {100, 100, 100};
uint8_t updated;
uint8_t state_updated;

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
  ops.property = EPCElectricRainDoorOpenCloseStatus;
  ops.length = 1;
  ops.data = &status_list[object->instance-1];
  echonet_packet_add_operation(&packet, &ops);

  echonet_send_packet(&packet, NULL); // Multicast
}

static int get_property(EchonetObjectConfig *object, EchonetOperation *ops) {
    int index = object->instance - 1;
    switch (ops->property) {
        case EPCCommonOperationStatus:
            ops->length = 1;
            ops->data[0] = 0x30;
            break;
        case EPCElectricRainDoorOpenCloseOperation:
            ops->length = 1;
            ops->data[0] = operation_list[index];
            break;
        case EPCElectricRainDoorOpenCloseStatus:
            ops->length = 1;
            ops->data[0] = status_list[index];
            break;
        default:
            Serial.printf("Invalid GET 0x%02x\r\n", ops->property);
            return -1;
    }
    return 0;
}

static int set_property(EchonetObjectConfig *object, EchonetOperation *ops) {
    int index = object->instance - 1;
    switch (ops->property) {
        case EPCElectricRainDoorOpenCloseOperation:
            printf("Instance %d, Set OpenCloseOperation 0x%02x\n", object->instance, ops->data[0]);
            switch(ops->data[0]) {
              case OperationStop:
                if(operation_list[index] != OperationStop) {
                  if(open_level[index] < 100 && open_level[index] > 0){
                    status_list[index] = StateStoppedHalfway;
                    state_updated = 1;
                  }
                }
            }
            operation_list[index] = ops->data[0];
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
    int x = 50 + 70 * i;
    int y = 36;

    char buf[32];
    sprintf(buf, "%3d%%", open_level[i]);

   int height = (100 - open_level[i]);
   int fill_height = 100;

    M5.Lcd.fillRect( x, y + 20, 20, fill_height, BLACK);
    M5.Lcd.drawRect( x, y + 20, 20, fill_height, WHITE);

    M5.Lcd.fillRect( x, y + 20, 20, height, fill_color);
    M5.Lcd.drawRect( x, y + 20, 20, height, line_color);
    M5.Lcd.drawString(buf, x, y);
  }
}

void setup(void)
{
  esp_log_level_set("*", ESP_LOG_DEBUG);
  M5.begin();
  Serial.begin(115200);

  M5.Lcd.println("connecting to WiFi");

  WiFi.begin("wifissid", "wifipassword");
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

  enconfig.objectCount = 3;
  enconfig.objects = objects;

  // Start Echonet Lite Task in background
  echonet_start_and_wait(&enconfig, portMAX_DELAY);

  Serial.println("Echonet Lite Started");

  M5.Lcd.clear();
  updated = 1;
  state_updated = 1;
}

void loop(void)
{
  M5.update();
  delay(100);
  for(uint8_t i; i<3; i++){
    switch (operation_list[i]) {
      case OperationClose:
        if( open_level[i]>0 ) {
          status_list[i] = StateClosing;
          open_level[i]--;
          Serial.printf("%d %3d%%\r\n", i, open_level[i]);
          updated = 1;
        }else if(status_list[i] != StateFullyClosed){
          status_list[i] = StateFullyClosed;
          operation_list[i] = OperationStop;
          Serial.printf("%d Fully Closed\r\n", i);
          state_updated = 1;
          updated = 1;
        }
        break;
      case OperationOpen:
        if( open_level[i]<100) {
          status_list[i] = StateOpening;
          open_level[i]++;
          Serial.printf("%d %3d%%\r\n", i, open_level[i]);
          updated = 1;
        }else if(status_list[i] != StateFullyOpen){
          status_list[i] = StateFullyOpen;
          operation_list[i] = OperationStop;
          Serial.printf("%d Fully Open\r\n", i);
          state_updated = 1;
          updated = 1;
        }
        break;
    }
  }

  if (state_updated) {
    state_updated = 0;
    send_inf(&objects[0]);
    send_inf(&objects[1]);
    send_inf(&objects[2]);
  }
  // 表示更新
  if( updated ){
    updated = 0;
    update_display();
  }

}
