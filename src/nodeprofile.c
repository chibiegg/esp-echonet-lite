#include "echonet.h"
#include "echonet_internal.h"


int _el_node_profile_get_property(EchonetOperation *resOps) {
    EchonetConfig *cfg = _el_get_config();

    switch (resOps->property) {
        case 0x80: // 動作状態
            resOps->length = 1;
            resOps->data[0] = 0x30;
            break;

        case 0x82: // 動作状態
            resOps->length = 4;
            resOps->data[0] = 0x01;
            resOps->data[1] = 0x00;
            resOps->data[2] = 0x01;
            resOps->data[3] = 0x00;
            break;

        case 0x83: // 識別
            resOps->length = 12;
            resOps->data[0] = 0xfe;
            resOps->data[1] = (uint8_t)((cfg->Vendor >> 16) & 0xff);
            resOps->data[2] = (uint8_t)((cfg->Vendor >> 8) & 0xff);
            resOps->data[3] = (uint8_t)(cfg->Vendor & 0xff);

            resOps->data[4] = (uint8_t)((cfg->Serial >> 56) & 0xff);
            resOps->data[5] = (uint8_t)((cfg->Serial >> 48) & 0xff);
            resOps->data[6] = (uint8_t)((cfg->Serial >> 40) & 0xff);
            resOps->data[7] = (uint8_t)((cfg->Serial >> 32) & 0xff);
            resOps->data[8] = (uint8_t)((cfg->Serial >> 24) & 0xff);
            resOps->data[9] = (uint8_t)((cfg->Serial >> 16) & 0xff);
            resOps->data[10] = (uint8_t)((cfg->Serial >> 8) & 0xff);
            resOps->data[11] = (uint8_t)(cfg->Serial & 0xff);
            break;

        case 0x8A: // メーカーコード
            resOps->length = 3;
            resOps->data[0] = (uint8_t)((cfg->Vendor >> 16) & 0xff);
            resOps->data[1] = (uint8_t)((cfg->Vendor >> 8) & 0xff);
            resOps->data[2] = (uint8_t)(cfg->Vendor & 0xff);
            break;
            
        case 0x8C: // 商品コード
            resOps->length = 8;
            resOps->data[0] = (uint8_t)((cfg->Product >> 56) & 0xff);
            resOps->data[1] = (uint8_t)((cfg->Product >> 48) & 0xff);
            resOps->data[2] = (uint8_t)((cfg->Product >> 40) & 0xff);
            resOps->data[3] = (uint8_t)((cfg->Product >> 32) & 0xff);
            resOps->data[4] = (uint8_t)((cfg->Product >> 24) & 0xff);
            resOps->data[5] = (uint8_t)((cfg->Product >> 16) & 0xff);
            resOps->data[6] = (uint8_t)((cfg->Product >> 8) & 0xff);
            resOps->data[7] = (uint8_t)(cfg->Product & 0xff);
            break;

        case 0xD3: // 自ノードインスタンス数
            resOps->length = 3;
            resOps->data[0] = (uint8_t)((cfg->objectCount >> 16) & 0xff);
            resOps->data[1] = (uint8_t)((cfg->objectCount >> 8) & 0xff);
            resOps->data[2] = (uint8_t)(cfg->objectCount & 0xff);
            break;

        case 0xD4: // 自ノードクラス数
            {
                int classCount = cfg->objectCount + 1; // ノードプロファイルクラスを含む FIXME: 重複を排除する
                resOps->length = 3;
                resOps->data[0] = (uint8_t)((classCount >> 16) & 0xff);
                resOps->data[1] = (uint8_t)((classCount >> 8) & 0xff);
                resOps->data[2] = (uint8_t)(classCount & 0xff);
            }
            break;
            
        case 0xD6: // 自ノードインスタンスリストS
            resOps->length = 1 + 3 * (cfg->objectCount);
            resOps->data[0] = (uint8_t)(cfg->objectCount);
            for (int i=0; i<cfg->objectCount; i++) {
                resOps->data[i*3+1] = (uint8_t)((cfg->objects[i].object >> 8) & 0xff);
                resOps->data[i*3+2] = (uint8_t)(cfg->objects[i].object & 0xff);
                resOps->data[i*3+3] = cfg->objects[i].instance;
            }
            break;

        case 0xD7: // 自ノードクラスリストS
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
            return -1;
            break;
    }
    return 0;
}


int _el_node_profile_set_property(EchonetOperation *ops) {
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
    packet.operations[0].property = 0xD5; // インスタンスリスト通知
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
