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

#include "ProxyTCPProcessor.h"
#include "Log/Backend/LogManager.h"

L3::ProxyTCPProcessor::ProxyTCPProcessor(std::shared_ptr<L3::IL3ForPacketProcessor> l3) : ProxyPacketProcessor(l3) {
    _tcbMap = make_unique<LRUCache<ControlBlockKey, std::shared_ptr<ProxyTCB>, ControlBlockKey::Hasher>>(
            MAX_PROXY_TCB_COUNT);
    _tcbMap->attachCleanUpCallback([this](ControlBlockKey key, std::shared_ptr<ProxyTCB> tcb) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_TCP, "PROXY TCP OnCleanUp", "addr: " + tcb->key().dstAddress().addr() + " port:" + to_string(tcb->key().dstAddress().port()));
#endif
        _L3->onControlBlockDestroyed(key);
        _L3->onSessionClosed(tcb, false);
    });

    _timerTask = Anyfi::Timer::schedule(std::bind(&L3::ProxyTCPProcessor::_timerTick, this),
                                        PROXY_EXPIRE_TIMER_PERIOD, PROXY_EXPIRE_TIMER_PERIOD);
}

L3::ProxyTCPProcessor::~ProxyTCPProcessor() {
    shutdown();
}

void
L3::ProxyTCPProcessor::connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) {
    if (address.isEmpty())
        throw std::invalid_argument("Empty address");
    if (address.isP2PConnectionType())
        throw std::invalid_argument("Address could not be P2P connection type");
    if (address.isMeshProtocol())
        throw std::invalid_argument("Address could not be Mesh Protocol");

    _L3->connectFromPacketProcessor(address, [this, callback, address](unsigned short channelId) {
        if (channelId == 0) {
            callback(nullptr);
            return;
        }

        if (address.connectionType() != Anyfi::ConnectionType::Proxy)
            throw std::invalid_argument("Address should be proxy connection type");

        ControlBlockKey key(srcAddr(channelId), address);
        auto tcb = std::make_shared<ProxyTCB>(key);
        _addTCB(tcb, key);

        callback(tcb);
    });
}

void L3::ProxyTCPProcessor::read(NetworkBufferPointer buffer, Anyfi::Address src, unsigned short linkId) {
    Anyfi::LogManager::getInstance()->countProxyReadBuffer(buffer->getWritePos() - buffer->getReadPos());
    Anyfi::Address local;
    local.type(Anyfi::AddrType::IPv4);
    local.addr(PROXY_SESSION_LOCAL_ADDR);
    local.port(linkId);

    ControlBlockKey key(local, src);
    auto tcb = _findTCB(key);
    if (tcb == nullptr) {
        Anyfi::Log::error(__func__, "tcb is nullptr");
        return;
    }

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_TCP, "PROXY TCP READ", "type: " + to_string(key.dstAddress().networkType()) + " id: " + to_string(tcb->id())
                                                                               + " addr: " + key.dstAddress().addr() + " port:" + to_string(key.dstAddress().port())
                                                                               + " len: " + to_string(buffer->getWritePos() - buffer->getReadPos()));
#endif

    tcb->clearExpireTimerTicks();

    _L3->onReadFromPacketProcessor(tcb, buffer);
}

void L3::ProxyTCPProcessor::write(NetworkBufferPointer buffer, ControlBlockKey key) {
    auto tcb = _findTCB(key);
    if (tcb == nullptr) {
        Anyfi::Log::error("L3", "ProxyTCB is nullptr, ProxyTCP#write");
        return;
    }

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_TCP, "PROXY TCP WRITE", "type: " + to_string(key.dstAddress().networkType()) + " id: " + to_string(tcb->id())
                                                                                + " addr: " + key.dstAddress().addr() + " port:" + to_string(key.dstAddress().port())
                                                                                + " len: " + to_string(buffer->getWritePos() - buffer->getReadPos()));
#endif

    tcb->clearExpireTimerTicks();

    Anyfi::LogManager::getInstance()->countProxyWriteBuffer(buffer->getWritePos() - buffer->getReadPos());
    // Key.srcAddresss.port is channelId
    _L3->writeFromPacketProcessor(key.srcAddress().port(), buffer);
}

void L3::ProxyTCPProcessor::close(ControlBlockKey key, bool force) {
    auto tcb = _findTCB(key);
    if (tcb == nullptr)
        return;

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_TCP, "PROXY TCP CLOSE", "type: " + to_string(key.dstAddress().networkType()) + " id: " + to_string(tcb->id()) + " addr: " + key.dstAddress().addr() + " port:" + to_string(key.dstAddress().port()));
#endif

    _removeTCB(key);
}

void L3::ProxyTCPProcessor::shutdown() {
    if (_timerTask != nullptr) {
        Anyfi::Timer::cancel(_timerTask);
        _timerTask = nullptr;
    }

}

void L3::ProxyTCPProcessor::_addTCB(const std::shared_ptr<ProxyTCB> &tcb, ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_tcbMapMutex);
        _tcbMap->put(key, tcb);
    }

    _L3->onControlBlockCreated(tcb);
}

void L3::ProxyTCPProcessor::_removeTCB(ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_tcbMapMutex);
        _tcbMap->remove(key);
    }

    _L3->onControlBlockDestroyed(key);
}

std::shared_ptr<ProxyTCB> L3::ProxyTCPProcessor::_findTCB(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_tcbMapMutex);

    if (_tcbMap->exists(key))
        return _tcbMap->get(key);

    return nullptr;
}


std::unordered_map<ControlBlockKey, std::shared_ptr<ProxyTCB>, ControlBlockKey::Hasher>
L3::ProxyTCPProcessor::_copyTCBMap() {
    Anyfi::Config::lock_type guard(_tcbMapMutex);

    auto map = std::unordered_map<ControlBlockKey, std::shared_ptr<ProxyTCB>, ControlBlockKey::Hasher>();
    auto it = _tcbMap->listIterBegin();
    while (it != _tcbMap->listIterEnd()) {
        auto cache = *it++;
        auto key = cache.first;
        auto tcb = cache.second;
        map[key] = tcb;
    }

    return map;
}

void L3::ProxyTCPProcessor::_timerTick() {
    auto tcbMap = _copyTCBMap();

    for (auto &item: tcbMap) {
        auto tcb = item.second;
        tcb->addExpireTimerTicks();

        auto expiredTick = PROXY_EXPIRE_TIME / PROXY_EXPIRE_TIMER_PERIOD;
        if (tcb->expireTimerTicks() >= expiredTick) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_TCP, "PROXY TCP OnExpire", "addr: " + tcb->key().dstAddress().addr() + " port:" + to_string(tcb->key().dstAddress().port()));
#endif
            // Expired
            _removeTCB(tcb->key());

            _L3->onSessionClosed(tcb, true);
        }
    }
}