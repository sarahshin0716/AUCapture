/*
 *  Copyright (c) 2016. Anyfi Inc.
 *
 *  All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Anyfi Inc. and its suppliers, if any.  The intellectual and technical concepts
 *  contained herein are proprietary to Anyfi Inc. and its suppliers and may be
 *  covered by U.S. and Foreign Patents, patents in process, and are protected
 *  by trade secret or copyright law.
 *
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from Anyfi Inc.
 */

#pragma once

#include <vector>

#include "../../../Common/Network/Address.h"
#include "../../../Common/Network/Buffer/NetworkBuffer.h"

/**
 * 패킷 형태
 * |--------------------------------|
 * | 1 byte | 4 bytes      | ...... |
 * |--------------------------------|
 * | Type   | PacketLength | ...... |
 * |--------------------------------|
 */
/*
 * 주의: mesh read/write link를 통해 본 패킷을 주고 받기 때문에 mesh packet과 형태가 동일해야 한다
 * handshake 이후 본 packet이 mesh packet과 섞여서 가는 경우가 있다. 개선 필요
 */
class P2PLinkLinkCheckPacket {
public:
    enum Type : uint8_t {
        RLinkCheck = 0xFE,
        WLinkCheck = 0xFF,
    };
    Type _type;
    uint8_t _link;

    explicit P2PLinkLinkCheckPacket(Type type, uint8_t link);
    explicit P2PLinkLinkCheckPacket(NetworkBufferPointer buffer);

    static Type getType(NetworkBufferPointer buffer);
    NetworkBufferPointer toPacket();
};
