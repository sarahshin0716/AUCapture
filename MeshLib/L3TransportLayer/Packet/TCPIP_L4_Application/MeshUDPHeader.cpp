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

#include "MeshUDPHeader.h"


//
// ========================================================================================
// MeshUDPHeader
//
unsigned short MeshUDPHeader::length() const {
    return MESH_UDP_HEADER_LENGTH;
}

uint32_t MeshUDPHeader::reserved() const {
    return _buffer->get<uint32_t>(_comptPos<uint32_t>(0)) & 0xFFFFFF;
}

void MeshUDPHeader::reserved(uint32_t reserved) {
    auto first1Byte = _buffer->get<uint8_t>(_comptPos<uint8_t>(0));
    _buffer->put<uint32_t>((first1Byte << 24) | (reserved & 0xFFFFFF), _comptPos<uint32_t>(0));
}
//
// MeshUDPHeader
// ========================================================================================
//


//
// ========================================================================================
// MeshUDPHeaderBuilder
//
void MeshUDPHeaderBuilder::sourcePort(uint16_t sourcePort) {
    _sourcePort = sourcePort;
}

void MeshUDPHeaderBuilder::destPort(uint16_t destPort) {
    _destPort = destPort;
}

std::shared_ptr<MeshUDPHeader> MeshUDPHeaderBuilder::build(NetworkBufferPointer buffer, unsigned short offset) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    if (buffer->size() < offset + MESH_UDP_HEADER_LENGTH) {
        auto writePos = buffer->getWritePos();
        buffer->resize(offset + MESH_UDP_HEADER_LENGTH);
        buffer->setWritePos(writePos);
    }

    auto meshUDPHeader = std::make_shared<MeshUDPHeader>(buffer, offset);
    meshUDPHeader->protocol(MeshProtocol::UDP);
    meshUDPHeader->reserved(0);
    meshUDPHeader->sourcePort(_sourcePort);
    meshUDPHeader->destPort(_destPort);

    if (buffer->getWritePos() < offset + MESH_UDP_HEADER_LENGTH)
        buffer->setWritePos(offset + MESH_UDP_HEADER_LENGTH);

    return meshUDPHeader;
}
//
// MeshUDPHeaderBuilder
// ========================================================================================
//