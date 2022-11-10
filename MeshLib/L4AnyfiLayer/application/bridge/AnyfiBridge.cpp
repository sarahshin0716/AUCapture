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

#include "AnyfiBridge.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"

void L4::AnyfiBridge::connect(Anyfi::Address connectAddr, std::function<void(unsigned short)> callback) {
    std::function<void(unsigned short)> onConnectedCallback = [this, callback](unsigned short newCbId){
        switch (_type) {
            case VpnToMesh:
                meshCbId(newCbId);
                break;
            case VpnToProxy:
            case MeshToProxy:
                proxyCbId(newCbId);
                break;
            default:
                break;
        }
        callback(newCbId);
    };
    _L4->connectSession(connectAddr, onConnectedCallback);
}

void L4::AnyfiBridge::read(NetworkBufferPointer buffer) {
    auto sourceCbId = getSourceCbId();

    if (sourceCbId != 0) {
        _L4->write(sourceCbId, buffer);
    } else {
        Anyfi::Log::error("L4::AnyfiBridge", std::string(__func__)+":: cbId is 0");
    }
}

void L4::AnyfiBridge::write(NetworkBufferPointer buffer) {
    auto sourceCbId = getSourceCbId();
    auto destCbId = getDestCbId();

    if (destCbId != 0) {
        _L4->write(destCbId, buffer);
    }else {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "... _pushBuffer() " +to_string(sourceCbId) +" -> " +to_string(destCbId) + " (buffer size: " + to_string(buffer->getWritePos() - buffer->getReadPos()) + ")");
#endif
        _pushBuffer(buffer);
    }
}

void L4::AnyfiBridge::close(unsigned short cbId, bool callClose) {
    if (_meshCbId != 0) {
        if (_meshCbId == cbId) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "close mesh " + to_string(meshCbId()) + " close: " + (callClose ? "true" : "false"));
#endif
            if (callClose) {
                _L4->closeSession(_meshCbId, false);
            }
        }
        _meshCbId = 0;
    }
    if (_proxyCbId != 0) {
        if (_proxyCbId == cbId) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "close proxy " + to_string(proxyCbId()) + " close: " + (callClose ? "true" : "false"));
#endif
            if (callClose) {
                _L4->closeSession(_proxyCbId, false);
            }
        }
        _proxyCbId = 0;
    }
    if (_vpnCbId != 0) {
        if (_vpnCbId == cbId) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "close vpn " + to_string(vpnCbId()) + " close: " + (callClose ? "true" : "false"));
#endif
            if (callClose) {
                _L4->closeSession(_vpnCbId, false);
            }
        }
        _vpnCbId = 0;
    }
    _clearBuffer();
}

void L4::AnyfiBridge::closeAll(bool forced) {
    if (_meshCbId != 0) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "close mesh " + to_string(meshCbId()) + " forced: " + (forced ? "true" : "false"));
#endif
        _L4->closeSession(_meshCbId, forced);
        _meshCbId = 0;
    }
    if (_proxyCbId != 0) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "close proxy " + to_string(proxyCbId()) + " forced: " + (forced ? "true" : "false"));
#endif
        _L4->closeSession(_proxyCbId, forced);
        _proxyCbId = 0;
    }
    if (_vpnCbId != 0) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "close vpn " + to_string(vpnCbId()) + " forced: " + (forced ? "true" : "false"));
#endif
        _L4->closeSession(_vpnCbId, forced);
        _vpnCbId = 0;
    }
    _clearBuffer();
}

void L4::AnyfiBridge::writeAllInQueue() {
    auto sourceCbId = getSourceCbId();
    auto destCbId = getDestCbId();

    if (destCbId != 0) {
        auto writeQueue = _popAllBuffer();
        while (!writeQueue.empty()) {
            auto buffer = writeQueue.front();
            writeQueue.pop();
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "... _popBuffer!! " +to_string(sourceCbId) + " -> " +to_string(destCbId) + " (buffer size: " + to_string(buffer->getWritePos() - buffer->getReadPos()) + ")");
#endif
            _L4->write(destCbId, buffer);
        }
    }
}

unsigned short L4::AnyfiBridge::getSourceCbId() {
    unsigned short sourceCbId = 0;
    switch (_type) {
        case VpnToMesh:
        case VpnToProxy:
            sourceCbId = _vpnCbId;
            break;
        case MeshToProxy:
            sourceCbId = _meshCbId;
            break;
        default:
            break;
    }
    return sourceCbId;
}

unsigned short L4::AnyfiBridge::getDestCbId() {
    unsigned short destCbId = 0;
    switch (_type) {
        case VpnToMesh:
            destCbId = _meshCbId;
            break;
        case VpnToProxy:
        case MeshToProxy:
            destCbId = _proxyCbId;
            break;
        default:
            break;
    }
    return destCbId;
}

void L4::AnyfiBridge::_pushBuffer(NetworkBufferPointer buffer) {
    Anyfi::Config::lock_type guard(_writeBufferQueueMutex);
    _writeBufferQueue.push(buffer);
}

std::queue<NetworkBufferPointer> L4::AnyfiBridge::_popAllBuffer() {
    Anyfi::Config::lock_type guard(_writeBufferQueueMutex);
    return _popAllBufferWithoutLock();
}

std::queue<NetworkBufferPointer> L4::AnyfiBridge::_popAllBufferWithoutLock() {
    std::queue<NetworkBufferPointer> bufferQueue;
    std::swap(_writeBufferQueue, bufferQueue);
    return bufferQueue;
}

void L4::AnyfiBridge::_clearBuffer() {
    Anyfi::Config::lock_type guard(_writeBufferQueueMutex);

    auto bufferQueue = _popAllBufferWithoutLock();
    while (!bufferQueue.empty()) {
        auto buffer = bufferQueue.front();
        bufferQueue.pop();
    }
}