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

#include "MeshProtocolHeader.h"


MeshProtocol MeshProtocolHeader::protocol() const {
    return MeshProtocol(_buffer->get<uint8_t>(_comptPos<uint8_t>(0)));
}

void MeshProtocolHeader::protocol(MeshProtocol protocol) {
    _buffer->put<uint8_t>(static_cast<uint8_t>(protocol), _comptPos<uint8_t>(0));
}

uint16_t MeshProtocolHeader::sourcePort() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(4));
}

void MeshProtocolHeader::sourcePort(uint16_t sourcePort) {
    _buffer->put<uint16_t>(sourcePort, _comptPos<uint16_t>(4));
}

uint16_t MeshProtocolHeader::destPort() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(6));
}

void MeshProtocolHeader::destPort(uint16_t destPort) {
    _buffer->put<uint16_t>(destPort, _comptPos<uint16_t>(6));
}
