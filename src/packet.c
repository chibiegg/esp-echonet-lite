#include "echonet.h"
#include "echonet_internal.h"


#define ECHONET_POS_EHD1		0
#define ECHONET_POS_EHD2		1
#define ECHONET_POS_TID		2
#define ECHONET_POS_SEOJ		4
#define ECHONET_POS_DEOJ		7
#define ECHONET_POS_ESV		10
#define ECHONET_POS_OPC		11
#define ECHONET_POS_OPS     12

#define ECHONET_POS_OPS_EPC     0
#define ECHONET_POS_OPS_PDC     1
#define ECHONET_POS_OPS_EDT     2

typedef struct {
    uint16_t transactionId;
    SemaphoreHandle_t semaphore;
    EchonetPacket packet;
    uint8_t buf[OPBUF_SIZE];
} ResponseWaiting;



uint16_t _lastTransactionId;
ResponseWaiting _responseWaitings[CONFIG_EL_MAX_WAITING_RESPONSE] = {0};
SemaphoreHandle_t _responseWaitingsMutex = NULL;

SemaphoreHandle_t _sendPacketMutex = NULL;

uint16_t get_next_transaction_id() {
    _lastTransactionId += 1;
    if (_lastTransactionId == 0) {
        _lastTransactionId = 1;
    }
    return _lastTransactionId;
}

void set_next_transaction_id(uint16_t transactionId) {
    _lastTransactionId  = transactionId - 1;
}

void echonet_prepare_packet(EchonetPacket *packet) {
    memset(packet, 0x00, sizeof(EchonetPacket));
    packet->transactionId = get_next_transaction_id();
}

int _el_parse_packet(uint8_t *buf, int length, EchonetPacket *packet) {
    if (buf == NULL) {
        return -1;
    }

    if (length < 12) {
        ESP_LOGW(TAG, "too short, 12");
        return -1;
    }

    if (buf[ECHONET_POS_EHD1] != 0x10 || buf[ECHONET_POS_EHD2] != 0x81) {
        ESP_LOGW(TAG, "invalid EHD 0x%02x%02x", buf[ECHONET_POS_EHD1], buf[ECHONET_POS_EHD2]);
        return -1;
    }

    memset(packet, 0x00, sizeof(EchonetPacket));
    packet->transactionId = ((int16_t)buf[ECHONET_POS_TID] << 8) | buf[ECHONET_POS_TID+1];
    packet->srcEchonetObject = ((int16_t)buf[ECHONET_POS_SEOJ] << 8) | buf[ECHONET_POS_SEOJ+1];
    packet->srcInstance = buf[ECHONET_POS_SEOJ+2];
    packet->dstEchonetObject = ((int16_t)buf[ECHONET_POS_DEOJ] << 8) | buf[ECHONET_POS_DEOJ+1];
    packet->dstInstance = buf[ECHONET_POS_DEOJ+2];
    packet->service = buf[ECHONET_POS_ESV];
    packet->operationCount = buf[ECHONET_POS_OPC];

    int pos = ECHONET_POS_OPS;
    for(int i=0; i<packet->operationCount; i++) {
        if(length < pos+2) {
            ESP_LOGW(TAG, "too short, ops");
            return -1;
        }

        uint8_t epc = buf[pos+ECHONET_POS_OPS_EPC];
        uint8_t pdc = buf[pos+ECHONET_POS_OPS_PDC];

        if (length < (pos+2+pdc)) {
            ESP_LOGW(TAG, "too short, edt");
            return -1;
        }

        packet->operations[i].property = epc;
        packet->operations[i].length = pdc;
        if (pdc > 0) {
            packet->operations[i].data = &buf[pos+ECHONET_POS_OPS_EDT];
        }

        pos += 2+pdc;
    }

    return 0;
}

void echonet_build_packet(uint8_t *buf, int *length, EchonetPacket *packet) {

    buf[ECHONET_POS_EHD1] = 0x10;
    buf[ECHONET_POS_EHD2] = 0x81;

    buf[ECHONET_POS_TID] = packet->transactionId >> 8;
    buf[ECHONET_POS_TID+1]= packet->transactionId & 0xff;
    buf[ECHONET_POS_SEOJ] = packet->srcEchonetObject  >> 8;
    buf[ECHONET_POS_SEOJ+1] = packet->srcEchonetObject & 0xff;
    buf[ECHONET_POS_SEOJ+2] = packet->srcInstance;
    buf[ECHONET_POS_DEOJ] = packet->dstEchonetObject  >> 8;
    buf[ECHONET_POS_DEOJ+1] = packet->dstEchonetObject & 0xff;
    buf[ECHONET_POS_DEOJ+2] = packet->dstInstance;
    buf[ECHONET_POS_ESV] = packet->service;
    buf[ECHONET_POS_OPC] = packet->operationCount;

    uint8_t *pbuf = &buf[ECHONET_POS_OPS];
    for(int i=0; i<packet->operationCount; i++) {
        EchonetOperation *ops = &(packet->operations[i]);

        *pbuf = ops->property; pbuf++;
        *pbuf = ops->length; pbuf++;
        memcpy(pbuf, ops->data, ops->length);
        pbuf += ops->length;
    }

    *length = pbuf-buf;
}

void echonet_copy_packet(EchonetPacket *dst, uint8_t *buf, EchonetPacket *src){
    dst->transactionId = src->transactionId ;
    dst->srcEchonetObject = src->srcEchonetObject;
    dst->srcInstance = src->srcInstance;
    dst->dstEchonetObject = src->dstEchonetObject;
    dst->dstInstance = src->dstInstance;
    dst->service = src->service;
    dst->operationCount = src->operationCount;

    for (int i=0;i<dst->operationCount;i++) {
        dst->operations[i].property = src->operations[i].property;
        dst->operations[i].length = src->operations[i].length;
        if (dst->operations[i].length > 0) {
            dst->operations[i].data = buf;
            memcpy(buf, src->operations[i].data, dst->operations[i].length);
            buf += dst->operations[i].length;
        }
    }
}

int _el_check_waiting_packet(EchonetPacket *packet) {
    if (_responseWaitingsMutex == NULL) {
        return -1;
    }

    if (xSemaphoreTake(_responseWaitingsMutex, 100/portTICK_PERIOD_MS) != pdTRUE){
        return -1;
    }
    for (int i=0; i<CONFIG_EL_MAX_WAITING_RESPONSE; i++) {

        ResponseWaiting *rw = &_responseWaitings[i];

        if (rw->transactionId != packet->transactionId || rw->transactionId == 0) {
            continue;
        }

        switch(packet->service){
            case Set_Res:
            case Get_Res:
            case INFC_Res:
            case SetC_SNA:
            case SetI_SNA:
            case Get_SNA:
            case INF_SNA:
                break;
            default:
                return -1;
                break;
        }

        echonet_copy_packet(&rw->packet, (uint8_t *)(&rw->buf), packet);
        xSemaphoreGive(rw->semaphore);
        xSemaphoreGive(_responseWaitingsMutex);
        ESP_LOGI(TAG, "Use responseWaitings[%d]", i);
        return 0;
    }
    
    xSemaphoreGive(_responseWaitingsMutex);
    return -1;
}


void echonet_prepare_response_packet(EchonetPacket *request, EchonetPacket *response) {

    memset(response, 0x00, sizeof(EchonetPacket));

    response->transactionId = request->transactionId;
    response->srcEchonetObject = request->dstEchonetObject;
    response->srcInstance = request->dstInstance;
    response->dstEchonetObject = request->srcEchonetObject;
    response->dstInstance = request->srcInstance;
    response->operationCount = request->operationCount;

    switch (request->service) {
        case Get:
            response->service = Get_Res;
            break;
        case SetI:
        case SetC:
            response->service = Set_Res;
            break;
        case INFC:
            response->service = INFC_Res;
            break;
        case INF_REQ:
            response->service = INF;
            break;
        default:
            ESP_LOGW(TAG, "Invalid service for response 0x%02x", request->service);
            break;
    }
}

int echonet_send_packet(EchonetPacket *packet, struct sockaddr_in *addr) {

    if (_sendPacketMutex == NULL) {
        _sendPacketMutex = xSemaphoreCreateMutex();
    }


    EchonetConfig *cfg = _el_get_config();
    uint8_t sendbuf[MAX_PACKET_SIZE];
    int length = sizeof(sendbuf);

    echonet_build_packet(sendbuf, &length, packet);

    struct sockaddr_in mcAddr;
    if (addr == NULL) {
        mcAddr.sin_family = AF_INET;
        mcAddr.sin_len = 4;
        mcAddr.sin_addr.s_addr = inet_addr(ECHONET_MULTICAST_IPV4_ADDR);
        addr = &mcAddr;
    }
    addr->sin_port = htons(ECHONET_UDP_PORT);

    BaseType_t status = xSemaphoreTake(_sendPacketMutex, 1000/portTICK_PERIOD_MS);
    if (status != pdTRUE) {
        ESP_LOGE(TAG, "Send mutex timeout");
        return -1;
    }
    int err = sendto(cfg->sock, sendbuf, length, 0, (const struct sockaddr *)addr, sizeof(struct sockaddr_in));
    xSemaphoreGive(_sendPacketMutex);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending TID 0x%04x: errno %d", packet->transactionId, errno);
        return err;
    }
    ESP_LOGI(TAG, "Send packet successful TID: 0x%04x", packet->transactionId);

    return 0;
}


int echonet_send_packet_and_wait_response(EchonetPacket *request, struct sockaddr_in *addr, EchonetPacket *response, uint8_t *buf) {
    if (_responseWaitingsMutex == NULL) {
        _responseWaitingsMutex = xSemaphoreCreateMutex();
    }

    if (xSemaphoreTake(_responseWaitingsMutex, 5000/portTICK_PERIOD_MS) != pdTRUE){
        return -1;
    }

    ResponseWaiting *responseWaiting = NULL;
    for (int i=0; i<CONFIG_EL_MAX_WAITING_RESPONSE; i++) {
        ResponseWaiting *rw = &_responseWaitings[i];
        if (rw->transactionId != 0) {
            continue;
        }
        rw->transactionId = request->transactionId;
        memset(rw->buf, 0x00, sizeof(rw->buf));
        memset(&rw->packet, 0x00, sizeof(rw->packet));
        responseWaiting = rw;
    }

    if (responseWaiting->semaphore == NULL) {
        _responseWaitingsMutex = xSemaphoreCreateBinary();
    }

    uint16_t transactionId = request->transactionId;

    xSemaphoreTake(responseWaiting->semaphore, 0); // reset semaphore


    int err = echonet_send_packet(request, addr);
    if (err < 0) {
        return -1;
    }

    // Waiting for response
    BaseType_t status = xSemaphoreTake(responseWaiting->semaphore, 5000/portTICK_PERIOD_MS);

    if (status == pdTRUE && responseWaiting->transactionId == transactionId) {
        // OK
        ESP_LOGI(TAG, "Response received");
        echonet_copy_packet(response, buf, &responseWaiting->packet);
        responseWaiting->transactionId = 0;
        return 0;
    }

    responseWaiting->transactionId = 0;
    ESP_LOGE(TAG, "Response timeout");
    return -2;
}
