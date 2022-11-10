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

#include "IPHeader.h"


IPVersion IPHeader::version() const {
    return IPVersion(_buffer->get<uint8_t>(_comptPos<uint8_t>(0)) >> 4);
}

void IPHeader::version(IPVersion version) {
    uint8_t last4bit = _buffer->get<uint8_t>(_comptPos<uint8_t>(0)) & (uint8_t) 0b1111;
    _buffer->put<uint8_t>(last4bit | ((uint8_t) version << 4), _comptPos<uint8_t>(0));
}

IPVersion IPHeader::getVersionFromBuffer(const NetworkBufferPointer &buffer, unsigned short startPosition) {
    if (buffer->isBackwardMode())
        throw std::invalid_argument(
                "getVersionFromBuffer doesn't support backward networkMode of NetworkBuffer. Use getVersionFromByte instead of this");

    return IPVersion(buffer->get<uint8_t>(startPosition) >> 4);
}

IPVersion IPHeader::getVersionFromByte(uint8_t &byte) {
    return IPVersion(byte >> 4);
}