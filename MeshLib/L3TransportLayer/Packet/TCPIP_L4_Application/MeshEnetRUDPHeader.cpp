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

#include "MeshEnetRUDPHeader.h"


//
// ========================================================================================
// MeshEnetRUDPHeader
//
uint8_t MeshEnetRUDPHeader::type() const {
    return _buffer->get<uint8_t>(_comptPos<uint8_t>(1));
}

void MeshEnetRUDPHeader::type(uint8_t type) {
    _buffer->put<uint8_t>(type, _comptPos<uint8_t>(1));
}

uint16_t MeshEnetRUDPHeader::reserved() const {
    return _buffer->get<uint16_t>(_comptPos<uint8_t>(2));
}

void MeshEnetRUDPHeader::reserved(uint16_t reserved) {
    _buffer->put<uint16_t>(reserved, _comptPos<uint16_t>(2));
}
//
// MeshEnetRUDPHeader
// ========================================================================================
//


//
// ========================================================================================
// MeshEnetRUDPHeader Builder
//
void MeshEnetRUDPHeaderBuilder::type(uint8_t type) {
    _type = type;
}

void MeshEnetRUDPHeaderBuilder::sourcePort(uint16_t sourcePort) {
    _sourcePort = sourcePort;
}

void MeshEnetRUDPHeaderBuilder::destPort(uint16_t destPort) {
    _destPort = destPort;
}

std::shared_ptr<MeshEnetRUDPHeader>
MeshEnetRUDPHeaderBuilder::build(NetworkBufferPointer buffer, unsigned short offset) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    if (buffer->size() < offset + MESH_ENET_RUDP_HEADER_LENGTH) {
        auto writePos = buffer->getWritePos();
        buffer->resize(offset + MESH_ENET_RUDP_HEADER_LENGTH);
        buffer->setWritePos(writePos);
    }

    auto meshRUDPHeader = std::make_shared<MeshEnetRUDPHeader>(buffer, offset);
    meshRUDPHeader->protocol(MeshProtocol::RUDP);
    meshRUDPHeader->type(_type);
    meshRUDPHeader->reserved(0);
    meshRUDPHeader->sourcePort(_sourcePort);
    meshRUDPHeader->destPort(_destPort);

    if (buffer->getWritePos() < offset + MESH_ENET_RUDP_HEADER_LENGTH)
        buffer->setWritePos(offset + MESH_ENET_RUDP_HEADER_LENGTH);

    return meshRUDPHeader;
}
//
// MeshEnetRUDPHeader Builder
// ========================================================================================
//