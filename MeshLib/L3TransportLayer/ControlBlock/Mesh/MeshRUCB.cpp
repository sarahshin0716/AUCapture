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

#include "MeshRUCB.h"

#include "Common/Network/Buffer/NetworkBufferPool.h"


MeshRUCB::MeshRUCB(ControlBlockKey &key) : MeshControlBlock(key), _state(State::CLOSED) {
    _key.srcAddress().connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);
    _key.dstAddress().connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);

    // Initialize timers
    for (auto &_timer : _timers)
        _timer = nullptr;
    initTimer(MeshRUDPTimer::Type::RETRANSMISSION);
    initTimer(MeshRUDPTimer::Type::DELAYED_ACK);
}

MeshRUCB::~MeshRUCB() {
    for (auto &_timer : _timers)
        delete _timer;
}

uint8_t MeshRUCB:: tickTimers() {
    uint8_t triggered = 0;
    for (auto &_timer : _timers) {
        if (_timer != nullptr)
            triggered |= _timer->tick();
    }
    return triggered;
}

void MeshRUCB::addWriteBuffer(NetworkBufferPointer buffer) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    _writeBuffers.push_back(buffer);
}

NetworkBufferPointer MeshRUCB::getWriteBuffer(unsigned int offset, unsigned short len) {
    if (len == 0)
        return nullptr;
    if (_writeBuffers.empty())
        return nullptr;

    auto writeBuffer = NetworkBufferPool::acquire();
    writeBuffer->setBackwardMode();

    auto iter = _writeBuffers.begin();
    while (iter != _writeBuffers.end()) {
        auto buffer = *iter++;
        if (buffer == nullptr)
            break;

        // Before offset
        if (buffer->getWritePos() < offset) {
            offset -= buffer->getWritePos();
            continue;
        }

        auto off = buffer->size() - buffer->getWritePos();
        if (buffer->getWritePos() - offset >= len) {
            writeBuffer->putBytes(buffer->buffer() + offset + off, len, 0);
            break;
        }

        len -= buffer->getWritePos() - buffer->getReadPos() - offset;
        if (iter == _writeBuffers.end()) {
            writeBuffer->setReadPos(len);
        }

        writeBuffer->putBytes(buffer->buffer() + offset + off, buffer->getWritePos() - offset, len);
        offset = 0;
    }

    // Nothing to write
    if (writeBuffer->size() == 0)
        return nullptr;

    writeBuffer->setWritePos(writeBuffer->size());

    return writeBuffer;
}

void MeshRUCB::dropWriteBuffer(unsigned short len) {
    if (_writeBuffers.empty())
        return;

    while (len > 0) {
        auto buffer = _writeBuffers.front();
        if (buffer == nullptr) {
            return;
        }
        if (buffer->getWritePos() > len) {
            buffer->setWritePos(buffer->getWritePos() - len);
            return;
        }

        len -= buffer->getWritePos() - buffer->getReadPos();
        _writeBuffers.pop_front();
    }
}

size_t MeshRUCB::writeBufferTotalLength() {
    if (_writeBuffers.empty())
        return 0;

    size_t totalLength = 0;

    auto iter = _writeBuffers.begin();
    while (iter != _writeBuffers.end()) {
        totalLength += (*iter)->getWritePos() - (*iter)->getReadPos();
        ++iter;
    }

    return totalLength;
}

void MeshRUCB::activateREMT() {
    addFlags(MeshRUCB::Flag::ACTIVATE_REMT);

    rexmtClear();
    _rexmtSeq = _seq;
}

void MeshRUCB::deactivateREMT() {
    removeFlags(MeshRUCB::Flag::ACTIVATE_REMT);

    rexmtClear();
}

unsigned int MeshRUCB::currentRTO() const {
    /**
     * Currently, used a fixed rto 600ms.
     * We should provide rtt based rto soon.
     */
    const unsigned short FIXED_RTO = 600;

    return FIXED_RTO * REXMT_BACKOFF[_rexmtShift];
}

uint32_t MeshRUCB::remainSendWindow() {
    return calculatedSendWindow() - unackedBytes();
}

uint32_t MeshRUCB::unackedBytes() {
    return _seq - _acked;
}
