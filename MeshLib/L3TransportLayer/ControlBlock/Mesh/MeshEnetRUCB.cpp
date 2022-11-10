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

#include "MeshEnetRUCB.h"

#include "../../../Common/Network/Buffer/NetworkBufferPool.h"


L3::MeshEnetRUCB::MeshEnetRUCB(ControlBlockKey &key) : MeshControlBlock(key), _state(State::CLOSED) {
    _key.srcAddress().connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);
    _key.srcAddress().connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);
}

L3::MeshEnetRUCB::~MeshEnetRUCB() = default;


void L3::MeshEnetRUCB::putReadBuffer(NetworkBufferPointer buffer) {
    if (buffer->isBackwardMode())
        throw std::invalid_argument("read buffer should be forward networkMode");

    Anyfi::Config::lock_type guard(_readBufMutex);

    _readBuffers.push_back(buffer);
}

NetworkBufferPointer L3::MeshEnetRUCB::popReadBuffer() {
    if (_readBuffers.size() == 0)
        return nullptr;

    Anyfi::Config::lock_type guard(_readBufMutex);

    auto ret = _readBuffers.front();
    _readBuffers.pop_front();

    return ret;
}

void L3::MeshEnetRUCB::putWriteBuffer(NetworkBufferPointer buffer) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("write buffer should be backward networkMode");

    Anyfi::Config::lock_type guard(_writeBufMutex);

    _writeBuffers.push_back(buffer);
}

void L3::MeshEnetRUCB::clearWriteBuffer() {
    Anyfi::Config::lock_type guard(_writeBufMutex);

    _writeBuffers.clear();
}
