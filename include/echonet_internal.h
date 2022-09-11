#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "sdkconfig.h"

#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define TAG "ENL"

#define ECHONET_MULTICAST_TTL         CONFIG_EL_MULTICAST_TTL
#define ECHONET_UDP_PORT              CONFIG_EL_PORT_NUM
#define ECHONET_MULTICAST_IPV4_ADDR   "224.0.23.0"
#define ECHONET_MULTICAST_IPV6_ADDR   "ff02::1"

#define OPBUF_SIZE       CONFIG_EL_MAX_OPERATION_BUFFER_SIZE
#define MAX_PACKET_SIZE  (12 + OPBUF_SIZE)

// socket
int _el_create_multicast_ipv4_socket(void);

// config
void _el_set_config(EchonetConfig *config);
EchonetConfig *_el_get_config();

// packet
int _el_receive_packet(int sock, char *recvbuf, int buflen, struct sockaddr_storage *raddr);
int _el_parse_packet(uint8_t *buf, int length, EchonetPacket *packet);
int _el_check_waiting_packet(EchonetPacket *packet);

// object
int _el_object_process_packet(EchonetObjectConfig *object, EchonetPacket *request, EchonetPacket *response, uint8_t *buf);
int _el_object_on_get(EchonetObjectConfig *object, EchonetPacket *request, EchonetPacket *response, uint8_t *buf);
int _el_object_on_set(EchonetObjectConfig *object, EchonetPacket *request, EchonetPacket *response, uint8_t *buf);

// nodeprofile
int _el_node_profile_get_property(EchonetOperation *resOps);
int _el_node_profile_set_property(EchonetOperation *ops);

