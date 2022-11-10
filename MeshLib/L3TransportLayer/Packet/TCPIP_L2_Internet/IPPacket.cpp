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

#include "IPPacket.h"

#include "IPHeader.h"
#include "IPv4Header.h"
#include "IPv6Header.h"


void IPPacket::_initializeIPHeader() {
    IPVersion version;
    if (_buffer->isBackwardMode()) {
        version = IPHeader::getVersionFromByte(_buffer->buffer()[0]);
    } else {
        version = IPHeader::getVersionFromBuffer(_buffer, _startPosition);
    }

    if (version == IPVersion::Version4) {
        if (_buffer->isBackwardMode()) {
            unsigned short ihl = IPv4Header::getIHLFromByte(_buffer->buffer()[0]);
            _header = std::make_shared<IPv4Header>(_buffer, _buffer->size() - ihl, ihl);
        } else _header = std::make_shared<IPv4Header>(_buffer, _startPosition);
    } else if (version == IPVersion::Version6) {
        if (_buffer->isBackwardMode()) {
            _header = std::make_shared<IPv6Header>(_buffer);
        } else _header = std::make_shared<IPv6Header>(_buffer, _startPosition);
    } else {
        // Not supported
        _header = nullptr;
    }
}

unsigned short IPPacket::size() const {
    IPVersion version = IPHeader::getVersionFromBuffer(_buffer, _startPosition);
    if (version == IPVersion::Version4) {
        IPv4Header *ipv4Header = dynamic_cast<IPv4Header *>(_header.get());
        return ipv4Header->totalLength() - ipv4Header->length();
    } else {
        // TODO : Not yet supported
        return 0;
    }
}

std::shared_ptr<IPHeader> IPPacket::header() const {
    return std::dynamic_pointer_cast<IPHeader>(_header);
}
