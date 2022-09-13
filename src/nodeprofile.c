#include "echonet.h"
#include "echonet_internal.h"


int _el_node_profile_get_property(EchonetObjectConfig *object, EchonetOperation *resOps) {
    EchonetConfig *cfg = _el_get_config();

    switch (resOps->property) {
        case EPCCommonOperationStatus: // 動作状態
            resOps->length = 1;
            resOps->data[0] = 0x30;
            break;

        case EPCNodeProfileSelfNodeInstances: // 自ノードインスタンス数
            resOps->length = 3;
            resOps->data[0] = (uint8_t)((cfg->objectCount >> 16) & 0xff);
            resOps->data[1] = (uint8_t)((cfg->objectCount >> 8) & 0xff);
            resOps->data[2] = (uint8_t)(cfg->objectCount & 0xff);
            break;

        case EPCNodeProfileSelfNodeClasses: // 自ノードクラス数
            {
                int classCount = cfg->objectCount + 1; // ノードプロファイルクラスを含む FIXME: 重複を排除する
                resOps->length = 3;
                resOps->data[0] = (uint8_t)((classCount >> 16) & 0xff);
                resOps->data[1] = (uint8_t)((classCount >> 8) & 0xff);
                resOps->data[2] = (uint8_t)(classCount & 0xff);
            }
            break;

        case EPCNodeProfileInstanceListNotification:
            resOps->length = 1;
            resOps->data[0] = 0;
            break;

        case EPCNodeProfileSelfNodeInstanceListS: // 自ノードインスタンスリストS
            resOps->length = 1 + 3 * (cfg->objectCount);
            resOps->data[0] = (uint8_t)(cfg->objectCount);
            for (int i=0; i<cfg->objectCount; i++) {
                resOps->data[i*3+1] = (uint8_t)((cfg->objects[i].object >> 8) & 0xff);
                resOps->data[i*3+2] = (uint8_t)(cfg->objects[i].object & 0xff);
                resOps->data[i*3+3] = cfg->objects[i].instance;
            }
            break;

        case EPCNodeProfileSelfNodeClassListS: // 自ノードクラスリストS
            {
                int classCount = cfg->objectCount; // ノードプロファイルクラスを含む FIXME: 重複を排除する
                resOps->length = 1 + 2 * (cfg->objectCount);
                resOps->data[0] = classCount;
                for (int i=0; i<cfg->objectCount; i++) {
                    resOps->data[i*2+1] = (uint8_t)((cfg->objects[i].object >> 8) & 0xff);
                    resOps->data[i*2+2] = (uint8_t)(cfg->objects[i].object & 0xff);
                }
            }
            break;

        default:
            ESP_LOGW(TAG, "NodeProfile unsupported EPC 0x%02x", resOps->property);
            return -1;
            break;
    }
    return 0;
}


int _el_node_profile_set_property(EchonetObjectConfig *object, EchonetOperation *ops) {
    return -1;
}


int echonet_nodeprofile_send_inf() {
    EchonetConfig *cfg = _el_get_config();

    EchonetPacket packet;
    echonet_prepare_packet(&packet);
    packet.srcEchonetObject = EOJNodeProfile;
    packet.srcInstance = 1;
    packet.dstEchonetObject = EOJNodeProfile;
    packet.dstInstance = 1;
    packet.service = INF;
    packet.operationCount = 1;

    uint8_t buf[OPBUF_SIZE];
    packet.operations[0].property = EPCNodeProfileInstanceListNotification; // インスタンスリスト通知
    packet.operations[0].length = 1 + 3 * (cfg->objectCount);
    packet.operations[0].data = buf;
    buf[0] = (uint8_t)(cfg->objectCount);
    for (int i=0; i<cfg->objectCount; i++) {
        buf[i*3+1] = (uint8_t)((cfg->objects[i].object >> 8) & 0xff);
        buf[i*3+2] = (uint8_t)(cfg->objects[i].object & 0xff);
        buf[i*3+3] = cfg->objects[i].instance;
    }

    return echonet_send_packet(&packet, NULL);
}
