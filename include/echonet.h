#ifndef _ECHONET_H_
#define _ECHONET_H_


#include "sdkconfig.h"
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
    EchonetOperation operations[CONFIG_EL_MAX_OPERATION_COUNT];
} EchonetPacket;

typedef struct {
    char release;
    uint8_t revision;
} EchonetProtocol;

typedef struct _EchonetObjectHooks EchonetObjectHooks;

typedef struct {
    uint16_t object;
    uint8_t instance;
    EchonetObjectHooks *hooks;
    EchonetProtocol protocol;
    uint16_t manufacturer;
    uint64_t product;
    uint64_t serialNumber;

    uint8_t *infPropertyMap;
    uint8_t *getPropertyMap;
    uint8_t *setPropertyMap;
} EchonetObjectConfig;

typedef struct _EchonetObjectHooks {
    int (*getProperty)(EchonetObjectConfig *object, EchonetOperation *ops);
    int (*setProperty)(EchonetObjectConfig *object, EchonetOperation *ops);
} EchonetObjectHooks;


typedef struct {
    int objectCount;
    EchonetObjectConfig *objects;

    uint16_t manufacturer;
    uint64_t product;
    uint64_t serialNumber;

    int _sock;
} EchonetConfig;

#ifdef __cplusplus
extern "C" {
#endif

void echonet_start(EchonetConfig *config);
int echonet_start_and_wait(EchonetConfig *config, portTickType xBlockTime);
int echonet_send_packet(EchonetPacket *packet, struct sockaddr_in *addr);
int echonet_send_packet_and_wait_response(
    EchonetPacket *request, struct sockaddr_in *addr,
    EchonetPacket *response, uint8_t *buf
);
void echonet_prepare_packet(EchonetPacket *packet);
void echonet_prepare_response_packet(EchonetPacket *request, EchonetPacket *response);
int echonet_packet_add_operation(EchonetPacket *packet, EchonetOperation *operation);
void echonet_copy_packet(EchonetPacket *dst, uint8_t *buf, EchonetPacket *src);
int echonet_build_protocol(EchonetProtocol *protocol, uint8_t *buf);
int echonet_nodeprofile_send_inf();

#ifdef __cplusplus
}
#endif

#endif /* _ECHONET_H_ */
