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

#include "MeshRUDPHeader.h"


//
// ========================================================================================
// MeshRUDPHeader
//
unsigned short MeshRUDPHeader::length() const {
    return MESH_RUDP_HEADER_LENGTH;
}

uint8_t MeshRUDPHeader::flags() const {
    return _buffer->get<uint8_t>(_comptPos<uint8_t>(1));
}

void MeshRUDPHeader::flags(uint8_t flags) {
    _buffer->put<uint8_t>(flags, _comptPos<uint8_t>(1));
}

uint16_t MeshRUDPHeader::windowSize() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(2));
}

void MeshRUDPHeader::windowSize(uint16_t windowSize) {
    _buffer->put<uint16_t>(windowSize, _comptPos<uint16_t>(2));
}

uint32_t MeshRUDPHeader::seqNum() const {
    return _buffer->get<uint32_t>(_comptPos<uint32_t>(8));
}

void MeshRUDPHeader::seqNum(uint32_t seqNum) {
    _buffer->put<uint32_t>(seqNum, _comptPos<uint32_t>(8));
}

uint32_t MeshRUDPHeader::ackNum() const {
    return _buffer->get<uint32_t>(_comptPos<uint32_t>(12));
}

void MeshRUDPHeader::ackNum(uint32_t ackNum) {
    _buffer->put<uint32_t>(ackNum, _comptPos<uint32_t>(12));
}

bool MeshRUDPHeader::isURG() const {
    return (flags() & MeshRUDPFlag::URG) == MeshRUDPFlag::URG;
}

bool MeshRUDPHeader::isACK() const {
    return (flags() & MeshRUDPFlag::ACK) == MeshRUDPFlag::ACK;
}

bool MeshRUDPHeader::isEAK() const {
    return (flags() & MeshRUDPFlag::EAK) == MeshRUDPFlag::EAK;
}

bool MeshRUDPHeader::isRST() const {
    return (flags() & MeshRUDPFlag::RST) == MeshRUDPFlag::RST;
}

bool MeshRUDPHeader::isSYN() const {
    return (flags() & MeshRUDPFlag::SYN) == MeshRUDPFlag::SYN;
}

bool MeshRUDPHeader::isFIN() const {
    return (flags() & MeshRUDPFlag::FIN) == MeshRUDPFlag::FIN;
}
//
// MeshRUDPHeader
// ========================================================================================
//


//
// ========================================================================================
// MeshRUDPHeader Builder
//
void MeshRUDPHeaderBuilder::flags(uint8_t flags) {
    _flags = flags;
}

void MeshRUDPHeaderBuilder::windowSize(uint16_t windowSize) {
    _windowSize = windowSize;
}

void MeshRUDPHeaderBuilder::seqNum(uint32_t seqNum) {
    _seqNum = seqNum;
}

void MeshRUDPHeaderBuilder::ackNum(uint32_t ackNum) {
    _ackNum = ackNum;
}

void MeshRUDPHeaderBuilder::sourcePort(uint16_t sourcePort) {
    _sourcePort = sourcePort;
}

void MeshRUDPHeaderBuilder::destPort(uint16_t destPort) {
    _destPort = destPort;
}

std::shared_ptr<MeshRUDPHeader>
MeshRUDPHeaderBuilder::build(NetworkBufferPointer buffer, unsigned short offset) {
    if (buffer->size() < offset + MESH_RUDP_HEADER_LENGTH) {
        auto writePos = buffer->getWritePos();
        auto readPos = buffer->getReadPos();
        buffer->resize(offset + MESH_RUDP_HEADER_LENGTH);
        buffer->setWritePos(writePos);
        buffer->setReadPos(readPos);
    }

    auto meshRudpHeader = std::make_shared<MeshRUDPHeader>(buffer, offset);
    meshRudpHeader->protocol(MeshProtocol::RUDP);
    meshRudpHeader->flags(_flags);
    meshRudpHeader->windowSize(_windowSize);
    meshRudpHeader->seqNum(_seqNum);
    meshRudpHeader->ackNum(_ackNum);
    meshRudpHeader->sourcePort(_sourcePort);
    meshRudpHeader->destPort(_destPort);

    if (buffer->getWritePos() < offset + MESH_RUDP_HEADER_LENGTH)
        buffer->setWritePos(offset + MESH_RUDP_HEADER_LENGTH);

    return meshRudpHeader;
}
//
// MeshRUDPHeader Builder
// ========================================================================================
//