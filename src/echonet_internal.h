#ifndef _ECHONET_INTERNAL_H_
#define _ECHONET_INTERNAL_H_

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define TAG "ENL"

#ifndef CONFIG_EL_PORT_NUM
#define CONFIG_EL_PORT_NUM (3610)
#define CONFIG_EL_MULTICAST_TTL (5)
#define CONFIG_EL_MAX_OPERATION_COUNT (10)
#define CONFIG_EL_MAX_OPERATION_BUFFER_SIZE (64)
#define CONFIG_EL_MAX_WAITING_RESPONSE (4)
#endif

#define ECHONET_MULTICAST_TTL CONFIG_EL_MULTICAST_TTL
#define ECHONET_UDP_PORT CONFIG_EL_PORT_NUM
#define ECHONET_MULTICAST_IPV4_ADDR "224.0.23.0"
#define ECHONET_MULTICAST_IPV6_ADDR "ff02::1"

#define MAX_OPERATION_COUNT CONFIG_EL_MAX_OPERATION_COUNT
#define OPBUF_SIZE CONFIG_EL_MAX_OPERATION_BUFFER_SIZE
#define MAX_PACKET_SIZE (12 + OPBUF_SIZE)

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
int _el_node_profile_get_property(EchonetObjectConfig *object, EchonetOperation *resOps);
int _el_node_profile_set_property(EchonetObjectConfig *object, EchonetOperation *ops);

#endif /* _ECHONET_INTERNAL_H_ */
