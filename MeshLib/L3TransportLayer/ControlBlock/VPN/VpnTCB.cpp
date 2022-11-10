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

#include "../../../L3TransportLayer/ControlBlock/Mesh/MeshControlBlock.h"
#include "../../../Common/Network/Buffer/NetworkBufferPool.h"
#include "VpnTCB.h"




VpnTCB::VpnTCB(ControlBlockKey &key) : VPNControlBlock(key), _state(TCBState::CLOSED) {
    _key.srcAddress().connectionProtocol(Anyfi::ConnectionProtocol::TCP);
    _key.dstAddress().connectionProtocol(Anyfi::ConnectionProtocol::TCP);

    seq_random();

    // Initialize timers
    for (auto &_timer : _timers)
        _timer = nullptr;
    initTimer(TCPTimer::Type::DELAYED_ACK);
}

VpnTCB::~VpnTCB() {
    for (auto &_timer : _timers)
        delete _timer;
}

uint8_t VpnTCB::tickTimers() {
    uint8_t triggered = 0;
    for (auto &_timer : _timers) {
        if (_timer != nullptr)
            triggered |= _timer->tick();
    }
    return triggered;
}

void VpnTCB::addReadBuffer(NetworkBufferPointer buffer) {
    if (buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be forward networkMode");

    _readBuffers.push_back(buffer);
}

void VpnTCB::addWriteBuffer(NetworkBufferPointer buffer) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    _writeBuffers.push_back(buffer);
}

NetworkBufferPointer VpnTCB::getWriteBuffer(unsigned int offset, unsigned short len) {
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

        // Before offset. Move to next buffer
        if (buffer->getWritePos() <= offset) {
            offset -= buffer->getWritePos();
            continue;
        }

        // If desired length is within the buffer, return chopped buffer
        auto off = buffer->size() - buffer->getWritePos();
        if (buffer->getWritePos() - offset >= len) {
            writeBuffer->putBytes(buffer->buffer() + offset + off, len, 0);
            break;
        }

        // Update desired length
        len -= buffer->getWritePos() - buffer->getReadPos() - offset;
        if (iter == _writeBuffers.end()) {
            writeBuffer->setReadPos(len);
        }

        writeBuffer->putBytes(buffer->buffer() + offset + off, buffer->getWritePos() - offset, len);
        offset = 0;
    }

    // Nothing to write
    if (writeBuffer->size() == 0) {
        return nullptr;
    }

    // Since buffer is backward networkMode, did absolute puts.
    // So we should set write position with size.
    writeBuffer->setWritePos(writeBuffer->size());

    return writeBuffer;
}

int VpnTCB::dropWriteBuffer(uint32_t len) {
    if (_writeBuffers.empty())
        return len;

    while (len > 0) {
        auto buffer = _writeBuffers.front();
        if (buffer == nullptr) {
            return len;
        }
        if (buffer->getWritePos() > len) {
            buffer->setWritePos(buffer->getWritePos() - len);
            return 0;
        }

        len -= buffer->getWritePos() - buffer->getReadPos();
        _writeBuffers.pop_front();
    }

    return len;
}

void VpnTCB::dropUnsentWriteBuffer() {
    if (_writeBuffers.empty())
        return;

    auto offset = unackedBytes();
    auto iter = _writeBuffers.begin();
    while (iter != _writeBuffers.end()) {
        auto buffer = *iter;
        if (buffer == nullptr || offset <= 0) {
            Anyfi::Log::error(__func__, "VPN TCP DROP: " + to_string(buffer->getWritePos() - buffer->getReadPos()));
            iter = _writeBuffers.erase(iter);
        } else if(buffer->getWritePos() <= offset) {
            offset -= buffer->getWritePos();
            iter++;
        } else {
            Anyfi::Log::error(__func__, "VPN TCP DROP(CHOP?): " + to_string(buffer->getWritePos()) + " offset: " + to_string(offset));
            /* TODO: chop buffer */
            offset = 0;
            iter++;
        }
    }
}

size_t VpnTCB::writeBufferTotalLength() {
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

double VpnTCB::remainSendWindow() {
    return calculatedSendWindow() - unackedBytes();
}

uint32_t VpnTCB::unackedBytes() {
    return _seq - _acked;
}
