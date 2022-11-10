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

#include "ProxyUDPProcessor.h"
#include "Log/Backend/LogManager.h"

L3::ProxyUDPProcessor::ProxyUDPProcessor(std::shared_ptr<L3::IL3ForPacketProcessor> l3) : ProxyPacketProcessor(l3) {
    _ucbMap = make_unique<LRUCache<ControlBlockKey, std::shared_ptr<ProxyUCB>, ControlBlockKey::Hasher>>(
            MAX_PROXY_UCB_COUNT);
    _ucbMap->attachCleanUpCallback([this](ControlBlockKey key, std::shared_ptr<ProxyUCB> ucb) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_UDP, "PROXY UDP OnCleanUp", "addr: " + ucb->key().dstAddress().addr() + " port:" + to_string(ucb->key().dstAddress().port()));
#endif
        _L3->onControlBlockDestroyed(key);
        _L3->onSessionClosed(ucb, false);
    });

    _timerTask = Anyfi::Timer::schedule(std::bind(&L3::ProxyUDPProcessor::_timerTick, this),
                                        PROXY_EXPIRE_TIMER_PERIOD, PROXY_EXPIRE_TIMER_PERIOD);
}

L3::ProxyUDPProcessor::~ProxyUDPProcessor() {
    shutdown();
}

void
L3::ProxyUDPProcessor::connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) {
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
        auto ucb = std::make_shared<ProxyUCB>(key);
        _addUCB(ucb, key);

#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_UDP, "PROXY UDP CONNECT", "addr: " + key.dstAddress().addr() + " port:" + to_string(key.dstAddress().port()));
#endif

        callback(ucb);
    });
}

void L3::ProxyUDPProcessor::read(NetworkBufferPointer buffer, Anyfi::Address src, unsigned short linkId) {
    Anyfi::LogManager::getInstance()->countProxyReadBuffer(buffer->getWritePos() - buffer->getReadPos());
    Anyfi::Address local;
    local.type(Anyfi::AddrType::IPv4);
    local.addr(PROXY_SESSION_LOCAL_ADDR);
    local.port(linkId);

    ControlBlockKey key(local, src);
    auto ucb = _findUCB(key);
    if (ucb == nullptr) {
        Anyfi::Log::error(__func__, "ucb is nullptr");
        return;
    }

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_UDP, "PROXY UDP READ", "addr: " + key.dstAddress().addr() + " port:" + to_string(key.dstAddress().port()) + " len: " + to_string(buffer->getWritePos() - buffer->getReadPos()));
#endif

    ucb->clearExpireTimerTicks();

    _L3->onReadFromPacketProcessor(ucb, buffer);
}

void L3::ProxyUDPProcessor::write(NetworkBufferPointer buffer, ControlBlockKey key) {
    auto ucb = _findUCB(key);
    if (ucb == nullptr) {
        Anyfi::Log::error(__func__, "ucb is nullptr");
        return;
    }

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_UDP, "PROXY UDP WRITE", "addr: " + key.dstAddress().addr() + " port:" + to_string(key.dstAddress().port()) + " len: " + to_string(buffer->getWritePos() - buffer->getReadPos()));
#endif

    ucb->clearExpireTimerTicks();

    Anyfi::LogManager::getInstance()->countProxyWriteBuffer(buffer->getWritePos() - buffer->getReadPos());
    // Key.srcAddresss.port is channelId
    _L3->writeFromPacketProcessor(key.srcAddress().port(), buffer);
}

void L3::ProxyUDPProcessor::close(ControlBlockKey key, bool force) {
    auto tcb = _findUCB(key);
    if (tcb == nullptr) {
        Anyfi::Log::error(__func__, "ucb is nullptr");
        return;
    }

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_UDP, "PROXY UDP OnClose", "addr: " + key.dstAddress().addr() + " port:" + to_string(key.dstAddress().port()));
#endif

    _removeUCB(key);
}

void L3::ProxyUDPProcessor::shutdown() {
    if (_timerTask != nullptr) {
        Anyfi::Timer::cancel(_timerTask);
        _timerTask = nullptr;
    }
}

void L3::ProxyUDPProcessor::_addUCB(const std::shared_ptr<ProxyUCB> &UCB, ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_ucbMapMutex);
        _ucbMap->put(key, UCB);
    }

    _L3->onControlBlockCreated(UCB);
}

void L3::ProxyUDPProcessor::_removeUCB(ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_ucbMapMutex);
        _ucbMap->remove(key);
    }

    _L3->onControlBlockDestroyed(key);
}

std::shared_ptr<ProxyUCB> L3::ProxyUDPProcessor::_findUCB(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_ucbMapMutex);

    if (_ucbMap->exists(key))
        return _ucbMap->get(key);

    return nullptr;
}

std::unordered_map<ControlBlockKey, std::shared_ptr<ProxyUCB>, ControlBlockKey::Hasher>
L3::ProxyUDPProcessor::_copyUCBMap() {
    Anyfi::Config::lock_type guard(_ucbMapMutex);

    auto map = std::unordered_map<ControlBlockKey, std::shared_ptr<ProxyUCB>, ControlBlockKey::Hasher>();
    auto it = _ucbMap->listIterBegin();
    while (it != _ucbMap->listIterEnd()) {
        auto cache = *it++;
        auto key = cache.first;
        auto ucb = cache.second;
        map[key] = ucb;
    }

    return map;
}

void L3::ProxyUDPProcessor::_timerTick() {
    auto ucbMap = _copyUCBMap();

    for (auto &item: ucbMap) {
        auto ucb = item.second;
        ucb->addExpireTimerTicks();

        auto expiredTick = PROXY_EXPIRE_TIME / PROXY_EXPIRE_TIMER_PERIOD;
        if (ucb->expireTimerTicks() >= expiredTick) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_PROXY_UDP, "PROXY UDP OnExpire", "addr: " + ucb->key().dstAddress().addr() + " port:" + to_string(ucb->key().dstAddress().port()));
#endif

            // Expired
            _removeUCB(ucb->key());

            _L3->onSessionClosed(ucb, true);
        }
    }
}
