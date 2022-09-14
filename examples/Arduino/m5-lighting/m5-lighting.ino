#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <echonet.h>

static int get_property(EchonetObjectConfig *object, EchonetOperation *ops);
static int set_property(EchonetObjectConfig *object, EchonetOperation *ops);
static void send_inf(EchonetObjectConfig *object);

WiFiMulti wifiMulti;
bool updated;

// Configure Node Profile and Objects
uint8_t infPropertyMap[] = {EPCMonoFunctionalLightingOperationStatus, 0x00};
uint8_t getPropertyMap[] = {
  EPCMonoFunctionalLightingOperationStatus, EPCMonoFunctionalLightingLightLevel,
  ECHONET_LITE_DEFAULT_GET_PROPERTY_MAP,
  0x00
};
uint8_t setPropertyMap[] = {EPCMonoFunctionalLightingOperationStatus, EPCMonoFunctionalLightingLightLevel, 0x00};

EchonetObjectHooks objectHooks = {
  .getProperty = get_property,
  .setProperty = set_property,
};

EchonetObjectConfig objects[] = {
    {
      .object = EOJMonoFunctionalLighting,
      .instance = 1,
      .hooks = &objectHooks,
      .protocol = {'M', 0x00},
      .infPropertyMap = infPropertyMap, 
      .getPropertyMap = getPropertyMap,
      .setPropertyMap = setPropertyMap,
    },
};

EchonetConfig enconfig = {
  .manufacturer = 0xffff,
  .product = 0x00000001,
  .objectCount = 1,
  .objects = objects,
};

uint8_t operation_status_list[1] = {0x30,};
uint8_t light_level_list[1] = {100, };


static void send_inf(EchonetObjectConfig *object) {
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
                updated = true;
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
            updated = true;
            break;
        default:
            ESP_LOGW(TAG, "Unsupported GET EPC 0x%02x", ops->property);
            return -1;
    }
    return 0;
}

void update_display(void) {
  int32_t width = M5.Display.width();
  int32_t height = M5.Display.height();

  M5.Display.startWrite();
  if (operation_status_list[0] == 0x30) {
    M5.Lcd.fillRect( 0, 0, width, height, WHITE);
  } else {
    M5.Lcd.fillRect( 0, 0, width, height, BLACK);
  }
  M5.Display.endWrite();
}

void setup() {
    M5.begin();
    M5.Power.begin();
    Serial.begin(115200);
  
    if (M5.Display.width() < M5.Display.height())
    { /// Landscape mode.
      M5.Display.setRotation(M5.Display.getRotation() ^ 1);
    }

    wifiMulti.addAP("M5-", "Of");

    Serial.print("\nConnecting Wifi");
    while(wifiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: "); 
    Serial.println(WiFi.macAddress());
  
    uint64_t serialNumber = 0;
    uint8_t *mac = (uint8_t *)&serialNumber + 2;
    WiFi.macAddress(mac);
    enconfig.serialNumber = serialNumber;
    enconfig.objects[0].serialNumber = serialNumber + 1;
  
    // Start Echonet Lite Task in background
    echonet_start_and_wait(&enconfig, portMAX_DELAY);
    Serial.println("Echonet Lite Started");
  
    M5.Lcd.clear();
    updated = true;
}

void loop() {
    M5.update();
    if (wifiMulti.run() == WL_CONNECTED) {

      if (M5.BtnA.wasPressed()) {
        if (operation_status_list[0] == 0x30)
          operation_status_list[0] = 0x31;
        else
          operation_status_list[0] = 0x30;
        updated = true;
      }

      // 表示更新
      if( updated ){
        updated = false;
        update_display();
        send_inf(&objects[0]);
        delay(100);
      }

    } else {
        Serial.println("WiFi not connected");
    }
    
}
