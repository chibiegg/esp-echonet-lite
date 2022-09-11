#include "echonet.h"
#include "echonet_internal.h"


/* Add a socket, either IPV4-only or IPV6 dual mode, to the IPV4
   multicast group */
static int socket_add_ipv4_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int err = 0;

    // Configure multicast address to listen to
    err = inet_aton(ECHONET_MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(TAG, "Configured IPV4 multicast address '%s' is invalid.", ECHONET_MULTICAST_IPV4_ADDR);
        // Errors in the return value have to be negative
        err = -1;
        goto err;
    }
    ESP_LOGD(TAG, "Configured IPV4 Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", ECHONET_MULTICAST_IPV4_ADDR);
    }

    if (assign_source_if) {
        // Assign the IPv4 multicast source interface, via its IP
        // (only necessary if this socket is IPV4 only)
        err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
                         sizeof(struct in_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
            goto err;
        }
    }

    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        goto err;
    }

 err:
    return err;
}

int _el_create_multicast_ipv4_socket(void)
{
    struct sockaddr_in saddr = { 0 };
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }
    ESP_LOGD(TAG, "Create socket %d", sock);

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(ECHONET_UDP_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
        goto err;
    }
    ESP_LOGD(TAG, "Bind socket");



    // Assign multicast TTL (set separately from normal interface TTL)
    uint8_t ttl = ECHONET_MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
        goto err;
    }
    ESP_LOGD(TAG, "Set IP_MULTICAST_TTL to %d", ttl);


    // this is also a listening socket, so add it to the multicast
    // group for listening...
    err = socket_add_ipv4_multicast_group(sock, true);
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to add IPv4 multicast group. Error %d", errno);
        goto err;
    }
    ESP_LOGD(TAG, "Add IPv4 multicast group");


    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}


int _el_receive_packet(int sock, char *recvbuf, int buflen, struct sockaddr_storage *raddr) {
    struct timeval tv = {
        .tv_sec = 2,
        .tv_usec = 0,
    };
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);


    int s = select(sock + 1, &rfds, NULL, NULL, &tv);
    if (s < 0) {
        ESP_LOGE(TAG, "Select failed: errno %d", errno);
        return -1; // Error
    }


    if (s > 0 && FD_ISSET(sock, &rfds)) {
        // Incoming datagram received
        char raddr_name[32] = { 0 };

        socklen_t socklen = sizeof(struct sockaddr_storage);
        int len = recvfrom(sock, recvbuf, buflen, 0,
                            (struct sockaddr *)raddr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "multicast recvfrom failed: errno %d", errno);
            return -1; // error
        }

        // Get the sender's address as a string
        if (raddr->ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)raddr)->sin_addr,
                        raddr_name, sizeof(raddr_name)-1);
        }
        if (raddr->ss_family== PF_INET6) {
            inet6_ntoa_r(((struct sockaddr_in6 *)raddr)->sin6_addr, raddr_name, sizeof(raddr_name)-1);
        }
        ESP_LOGI(TAG, "received %d bytes from %s:", len, raddr_name);

        return len;
    }

    return 0; // nothing data
}
