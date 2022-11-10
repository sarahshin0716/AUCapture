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

// VpnChannel-android.cpp should only be compiled on android.
#if defined(ANDROID) || defined(__ANDROID__)

#include "VpnChannel.h"

#include <thread>
#include <utility>
#include <unistd.h>
#include <Log/Frontend/Log.h>
#include <Log/Backend/LogManager.h>
#include "Common/Timer.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"


void __posixClose(unsigned short fd) {
    close(fd);
}


bool VpnChannel::open(Anyfi::Address address) {
    auto currFd = _tunFd;

    _tunFd = address.port();

    if (currFd == 0) {
        _startVpnThread();
    } else {
        // Close previous file descriptor
        __posixClose(currFd);
    }

    return true;
}

void VpnChannel::close() {
    state(State::CLOSED);

    Anyfi::LogManager::getInstance()->record()->disconnectVpnLink(_tunFd);
    __posixClose(_tunFd);

    Anyfi::LogManager::getInstance()->record()->disconnectVpnThread();
    _stopVpnThread();
}

void VpnChannel::write(NetworkBufferPointer buffer) {
    _addToWriteQueue(buffer);
}

void VpnChannel::onClose(std::shared_ptr<Link> link) {
    // should not be call
}

void VpnChannel::protect(int sock) {

}

int VpnChannel::_doWrite(NetworkBufferPointer buffer) {
    return Anyfi::Socket::write(_tunFd, std::move(buffer));
}

NetworkBufferPointer VpnChannel::_doRead() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->resize(buffer->capacity());

    auto readBytes = Anyfi::Socket::read(_tunFd, buffer);

    if (readBytes <= 0) {
//        if (readBytes < 0)
//            Anyfi::Log::warn(__func__, strerror(errno));

        return nullptr;
    }

    buffer->resize(readBytes);
    buffer->setWritePos(readBytes);
    return buffer;
}


#endif