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

#include "P2PLinkLinkCheckPacket.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"


P2PLinkLinkCheckPacket::P2PLinkLinkCheckPacket(P2PLinkLinkCheckPacket::Type type, uint8_t link) {
    _type = type;
    _link = link;
}

P2PLinkLinkCheckPacket::P2PLinkLinkCheckPacket(NetworkBufferPointer buffer) {
    _type = static_cast<P2PLinkLinkCheckPacket::Type>(buffer->get<uint8_t>());
    _link = buffer->get<uint8_t>();
}

NetworkBufferPointer P2PLinkLinkCheckPacket::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();
    buffer->put<uint8_t>((uint8_t)_link);
    buffer->put<uint8_t>((uint8_t)_type);
    return buffer;
}

P2PLinkLinkCheckPacket::Type P2PLinkLinkCheckPacket::getType(NetworkBufferPointer buffer) {
    return static_cast<P2PLinkLinkCheckPacket::Type>(buffer->get<uint8_t>(buffer->getReadPos()));
}
