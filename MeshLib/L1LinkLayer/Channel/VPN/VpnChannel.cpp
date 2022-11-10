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

#include <limits>

#include "Config.h"
#include "Core.h"
#include "VpnChannel.h"
#include "Log/Frontend/Log.h"
#include "Log/Backend/LogManager.h"
#include "L1LinkLayer/Link/SocketLink.h"

#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR | TARGET_OS_IPHONE
        #define iOS
    #elif TARGET_OS_MAC
        #define _TUN
    #endif
#elif __linux__
    #define _TUN
#else
// Undefined
#endif

#if defined(ANDROID) || defined(__ANDROID__)
    #include "VpnChannel-android.cpp"
#elif defined(_TUN)
    #include "VpnChannel-tun.cpp"
#elif iOS
    #include "VpnChannel-iOS.cpp"
#elif _WIN32
    #include "VpnChannel-windows.cpp"
#endif

using namespace std::chrono;

VpnChannel::~VpnChannel() {
    _stopVpnThread();
}

std::vector<int> VpnChannel::socks() {
    std::vector<int> socks = {_tunFd};
    return socks;
}

void VpnChannel::_addToWriteQueue(NetworkBufferPointer buffer) {
    Anyfi::Config::lock_type guard(_writeQueueMutex);

    _writeQueue.push_back(buffer);
}

inline static uint64_t now() {
    return (uint64_t) duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void VpnChannel::_vpnReadThreadImpl() {
    std::stringstream ss;
    ss << "vpnReadThread";
    pthread_setname_np(pthread_self(), ss.str().c_str());

    /**
     * Sleeps this read thread for battery optimization, when non-blocking read returns nullptr.
     * If non-blocking vpn read returns nullptr continuously, it will sleep longer.
     *
     * TODO : Optimize sleep time values
     */
    const int NUM_SLEEP_TIME_COUNT = 30;
    const int SLEEP_TIME[NUM_SLEEP_TIME_COUNT] = {
            5, 5, 5, 5, 5, 10, 10, 10, 10, 10,
            20, 20, 20, 20, 20, 50, 50, 50, 50, 50,
            100, 100, 100, 100, 500, 500, 500, 1000, 1000, 1500
    };
    const int MAX_SLEEP_TIME_SHIFT = NUM_SLEEP_TIME_COUNT - 1;

    int emptyReadCount = 0;

    std::shared_ptr<BaseChannel> this_ = shared_from_this();
    while (_isVpnThreadRunning && _state == CONNECTED) {
        /**
         * Read operation
         */
        auto buf = _doRead();
        if (buf == nullptr) {
            /**
             * Battery optimization.
             */
            //std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME[emptyReadCount]));
            std::this_thread::sleep_for(std::chrono::milliseconds(33));

//            if (emptyReadCount < MAX_SLEEP_TIME_SHIFT)
//                emptyReadCount++;

        } else {
//            emptyReadCount = 0;
            Anyfi::Core::getInstance()->enqueueTask([this, this_, buf]{
                this_->onRead(std::make_shared<SocketLink>(0, nullptr), buf);
            });
        }
    }
}

void VpnChannel::_vpnWriteThreadImpl() {
    std::stringstream ss;
    ss << "vpnWriteThread";
    pthread_setname_np(pthread_self(), ss.str().c_str());

    std::vector<NetworkBufferPointer> tempWriteBuffers;

    while (_isVpnThreadRunning && _state == CONNECTED) {
        {
            Anyfi::Config::lock_type guard(_writeQueueMutex);
            tempWriteBuffers = _writeQueue;
            _writeQueue.clear();
        }
        if (tempWriteBuffers.size() > 0) {
            for (const auto &buf : tempWriteBuffers) {
                int bytes = buf->getWritePos() - buf->getReadPos();
                int writtenBytes = _doWrite(buf);
                if (writtenBytes != bytes) {
                    if (writtenBytes == 0) {
                        Anyfi::Log::error(__func__, "L1 VPN written 0 bytes");
                        {
                            Anyfi::Config::lock_type guard(_writeQueueMutex);
                            _writeQueue.push_back(buf);
                        }
                    } else {
                        Anyfi::LogManager::getInstance()->record()->vpnLinkWriteError(bytes, writtenBytes);
                    }
                }
            }
            tempWriteBuffers.clear();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    }
}

void VpnChannel::_startVpnThread() {
    Anyfi::Config::lock_type guard(_vpnMutex);
    _startVpnThreadWithoutLock();
}

void VpnChannel::_startVpnThreadWithoutLock() {
    _stopVpnThreadWithoutLock();

    _isVpnThreadRunning = true;

    state(State::CONNECTED);

    _vpnReadThread.reset(new std::thread(std::bind(&VpnChannel::_vpnReadThreadImpl, this)));
    _vpnWriteThread.reset(new std::thread(std::bind(&VpnChannel::_vpnWriteThreadImpl, this)));
}

void VpnChannel::_stopVpnThread() {
    Anyfi::Config::lock_type guard(_vpnMutex);
    _stopVpnThreadWithoutLock();
}

void VpnChannel::_stopVpnThreadWithoutLock() {
    if (!_isVpnThreadRunning)
        return;
    _isVpnThreadRunning = false;

    if (_vpnReadThread != nullptr) {
        _vpnReadThread->join();
        _vpnReadThread = nullptr;
    }

    if (_vpnWriteThread != nullptr) {
        _vpnWriteThread->join();
        _vpnWriteThread = nullptr;
    }
}
