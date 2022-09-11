#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "echonet_consts.h"


typedef struct {
    uint8_t property;
    uint8_t length;
    uint8_t *data;
} EchonetOperation;

typedef struct {
    uint16_t transactionId;
    uint16_t srcEchonetObject;
    uint8_t srcInstance;
    uint16_t dstEchonetObject;
    uint8_t dstInstance;
    uint8_t service;
    uint8_t operationCount;
    EchonetOperation operations[10];
} EchonetPacket;


typedef struct {
    int (*getProperty)(EchonetOperation *ops);
    int (*setProperty)(EchonetOperation *ops);
} EchonetObjectHooks;


typedef struct {
    uint16_t object;
    uint8_t instance;
    EchonetObjectHooks *hooks;
    uint8_t *infPropertyMap;
    uint8_t *getPropertyMap;
    uint8_t *setPropertyMap;
} EchonetObjectConfig;

typedef struct {
    esp_netif_t *netif;
    int objectCount;
    EchonetObjectConfig *objects;

    uint16_t Vendor;
    uint64_t Product;
    uint64_t Serial;

    int sock;
} EchonetConfig;


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
