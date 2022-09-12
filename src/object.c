#include "echonet.h"
#include "echonet_internal.h"

int _el_object_process_packet(EchonetObjectConfig *object, EchonetPacket *request, EchonetPacket *response, uint8_t *buf){
    if (object != NULL) {
        switch (request->service) {
            case Get:
            case INF_REQ:
                return _el_object_on_get(object, request, response, buf);
                break;
            case SetC:
            case SetI:
                return _el_object_on_set(object, request, response, buf);
                break;
        }
    }
    return 0;
}

static int _set_property_map(uint8_t *buf, uint8_t *map) {
    uint8_t count = 0;
    uint8_t *p = map;
    for (count = 0, p = map; p != NULL && *p != 0x00; count++) {
        if (count < 15) {
            buf[1+count] = *p;
        }
        p++;
    }
    buf[0] = count;

    if (count < 16) {
        // 16個よりも少ない場合はそのまま
        return 1 + count;
    }

    // bitmap format
    memset(&buf[1], 0x00, 16);
    for (count = 0, p = map; p != NULL && *p != 0x00; count++) {
        int i = *p & 0x0f;
        int b = ((*p >> 8) & 0x0f) - 8;
        buf[i+1] |= (1 << b);
        p++;
    }
    return 17;
}

int _el_object_on_get(EchonetObjectConfig *object, EchonetPacket *request, EchonetPacket *response, uint8_t *buf) {
    uint8_t sna_service = 0;
    switch (request->service) {
        case Get:
            sna_service = Get_SNA;
            break;
        case INF_REQ:
            sna_service = INF_SNA;
            break;
        default:
            ESP_LOGE(TAG, "Invalid service 0x%02x", request->service);
            return -1;
            break;
    }

    echonet_prepare_response_packet(request, response);
    response->srcEchonetObject = object->object;
    response->srcInstance = object->instance;

    for (int i=0;i<request->operationCount;i++) {
        EchonetOperation *ops = &(request->operations[i]);
        EchonetOperation *resOps = &(response->operations[i]);
        resOps->property = ops->property;
        resOps->length = 0;
        resOps->data = buf;

        switch (ops->property) {

            case EPCCommonProtocol:
                if (object->object == EOJNodeProfile) {
                    // Version (NodeProfile Only)
                    resOps->length = 4;
                    resOps->data[0] = 0x01;
                    resOps->data[1] = 0x00;
                    resOps->data[2] = 0x01;
                    resOps->data[3] = 0x00;
                } else {
                    // Protocol Version
                    resOps->length = echonet_build_protocol(&object->protocol, (uint8_t *)(&resOps->data[0]));
                }
                break;
            case 0x9D: // 状況アナウンスプロパティマップ
                resOps->length = _set_property_map(resOps->data, object->infPropertyMap);
                break;

            case 0x9E: // Setプロパティマップ
                resOps->length = _set_property_map(resOps->data, object->setPropertyMap);
                break;

            case 0x9F: // Getプロパティマップ
                resOps->length = _set_property_map(resOps->data, object->getPropertyMap);
                break;

            default: // オブジェクト固有の処理
                {
                    if (object->hooks->getProperty == NULL){
                        resOps->length = 0;
                        resOps->data = NULL;
                        response->service = sna_service;
                    } else {
                        int err = object->hooks->getProperty(object, resOps);
                        if (err < 0){
                            resOps->length = 0;
                            resOps->data = NULL;
                            response->service = sna_service;
                        }
                    }
                }
                break;
        }

        buf += resOps->length;
    }

    return 1;
}


int _el_object_on_set(EchonetObjectConfig *object, EchonetPacket *request, EchonetPacket *response, uint8_t *buf) {
    uint8_t sna_service = 0;
    switch (request->service) {
        case SetC:
            sna_service = SetC_SNA;
            break;
        case SetI:
            sna_service = SetI_SNA;
            break;
        default:
            ESP_LOGE(TAG, "Invalid service 0x%02x", request->service);
            return -1;
            break;
    }

    echonet_prepare_response_packet(request, response);
    response->srcEchonetObject = object->object;
    response->srcInstance = object->instance;

    for (int i=0;i<request->operationCount;i++) {
        EchonetOperation *ops = &(request->operations[i]);
        EchonetOperation *resOps = &(response->operations[i]);
        resOps->property = ops->property;
        resOps->length = 0;
        resOps->data = NULL;

        switch (ops->property) {
            default:
                {
                    if (object->hooks->setProperty == NULL){
                        resOps->length = ops->length;
                        resOps->data = buf;
                        memcpy(buf, ops->data, ops->length);
                        response->service = sna_service;
                    }else{
                        int err = object->hooks->setProperty(object, ops);
                        if (err < 0){
                            resOps->length = ops->length;
                            resOps->data = buf;
                            memcpy(buf, ops->data, ops->length);
                            response->service = sna_service;
                        }
                    }
                }
        }

        buf += resOps->length;
    }

    if (request->service == SetI && response->service == Set_Res) {
        return 0;
    }
    return 1;
}

int echonet_build_protocol(EchonetProtocol *protocol, uint8_t *buf) {
    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = (uint8_t)(protocol->release);
    buf[3] = protocol->revision;
    return 4;
}