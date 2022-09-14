# ESP Echonet Lite Component

Framework for building Echonet Lite Node for ESP-IDF.

## Examples

### For Espressif IoT Development Framework (ESP-IDF)

* [Mono Function Lighting (単機能照明)](/examples/ESP-IDF/monofunctionallighting/main/main.c)

### For Arduino (and with PlatformIO)

* [Mono Function Lighting (単機能照明)](/examples/Arduino/monofunctionallighting/src/main.cpp)
* [Electric Rain Door (電動雨戸/シャッター)](/examples/Arduino/electricraindoor/src/main.cpp)

### For Arduino

* [M5 Mono Function Lighting (単機能照明)](/examples/Arduino/m5-lighting/m5-lighting.ino)

## API

### Functions

```C
void echonet_start(EchonetConfig *config);
int echonet_start_and_wait(EchonetConfig *config, portTickType xBlockTime);
int echonet_send_packet(EchonetPacket *packet, struct sockaddr_in *addr);
int echonet_send_packet_and_wait_response(
    EchonetPacket *request, struct sockaddr_in *addr, EchonetPacket *response, uint8_t *buf);
void echonet_prepare_packet(EchonetPacket *packet);
void echonet_prepare_response_packet(EchonetPacket *request, EchonetPacket *response);
int echonet_packet_add_operation(EchonetPacket *packet, EchonetOperation *operation);
void echonet_copy_packet(EchonetPacket *dst, uint8_t *buf, EchonetPacket *src);
int echonet_build_protocol(EchonetProtocol *protocol, uint8_t *buf);
int echonet_nodeprofile_send_inf();

```