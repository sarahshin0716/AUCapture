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

#include <Core.h>
#include "MeshUDPProcessor.h"

#include "Log/Frontend/Log.h"
#include "L3TransportLayer/Packet/TCPIP_L4_Application/MeshUDPHeader.h"


L3::MeshUDPProcessor::MeshUDPProcessor(std::shared_ptr<L3::IL3ForPacketProcessor> l3,
                                       Anyfi::UUID myUid)
        : MeshPacketProcessor(std::move(l3), myUid) {
    _ucbMap = make_unique<LRUCache<ControlBlockKey, std::shared_ptr<MeshUCB>, ControlBlockKey::Hasher>>(
            MAX_MESH_UCB_COUNT);
    _ucbMap->attachCleanUpCallback([this](ControlBlockKey key, std::shared_ptr<MeshUCB> ucb) {
        _L3->onControlBlockDestroyed(key);
        _L3->onSessionClosed(ucb, true);
    });

    _timerTask = Anyfi::Timer::schedule(std::bind(&L3::MeshUDPProcessor::_timerTick, this),
                                        MESH_UCB_EXPIRE_TIMER_PERIOD, MESH_UCB_EXPIRE_TIMER_PERIOD);
}

L3::MeshUDPProcessor::~MeshUDPProcessor() {
    shutdown();
}

void
L3::MeshUDPProcessor::connect(Anyfi::Address address,
                              std::function<void(std::shared_ptr<ControlBlock> cb)> callback) {
    if (address.type() != Anyfi::AddrType::MESH_UID)
        throw std::invalid_argument("Destination address type should be MESH_UID");
    if (address.connectionProtocol() != Anyfi::ConnectionProtocol::MeshUDP)
        throw std::invalid_argument("Connection protocol must be MeshUDP");

    Anyfi::Address src;
    src.setMeshUID(_myUid);
    src.port(_L3->assignPort());
    src.connectionType(Anyfi::ConnectionType::WifiP2P); //TODO: General P2P Type
    src.connectionProtocol(Anyfi::ConnectionProtocol::MeshUDP);

    ControlBlockKey key(src, address);
    auto ucb = std::make_shared<MeshUCB>(key);
    _addUCB(ucb, key);

    if (callback != nullptr)
        callback(ucb);
}

void L3::MeshUDPProcessor::read(NetworkBufferPointer buffer, Anyfi::Address src) {
    if (buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be forward networkMode.");

    auto header = std::make_shared<MeshUDPHeader>(buffer, buffer->getReadPos());
    src.port(header->sourcePort());
    src.connectionProtocol(Anyfi::ConnectionProtocol::MeshUDP);

    Anyfi::Address dst;
    dst.port(header->destPort());
    dst.setMeshUID(_myUid);
    dst.connectionType(Anyfi::ConnectionType::WifiP2P); //TODO: General P2P Type
    dst.connectionProtocol(Anyfi::ConnectionProtocol::MeshUDP);

    ControlBlockKey key(dst, src);
    auto ucb = _findUCB(key);
    if (ucb == nullptr) {
        // New session
        ucb = std::make_shared<MeshUCB>(key);
        _addUCB(ucb, key);
        _L3->onSessionConnected(ucb);
    }

    ucb->clearExpireTimerTicks();

    buffer->setReadPos(buffer->getReadPos() + header->length());
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_MESH_UDP, "MeshUDP", "read to " + to_string(ucb->id()) + " size: " + to_string(buffer->getWritePos() - buffer->getReadPos()));
    _L3->onReadFromPacketProcessor(ucb, buffer);
}

void L3::MeshUDPProcessor::write(NetworkBufferPointer buffer, ControlBlockKey key) {
    auto ucb = _findUCB(key);
    if (ucb == nullptr) {
        Anyfi::Log::error("L3", "Failed to write : UCB not found");
        return;
    }

    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_MESH_UDP, "MeshUDP", "write to " + to_string(ucb->id()) + " size: " + to_string(buffer->getWritePos() - buffer->getReadPos()));


    MeshUDPHeaderBuilder builder;
    builder.sourcePort(ucb->key().srcAddress().port());
    builder.destPort(ucb->key().dstAddress().port());
    builder.build(buffer, buffer->getWritePos());

    ucb->clearExpireTimerTicks();

    _L3->writeFromPacketProcessor(ucb->key().dstAddress(), buffer);
}

void L3::MeshUDPProcessor::close(ControlBlockKey key, bool force) {
    _removeUCB(key);
}

void L3::MeshUDPProcessor::shutdown() {
    _clearAllUCB();

    if (_timerTask != nullptr) {
        Anyfi::Timer::cancel(_timerTask);
        _timerTask = nullptr;
    }
}

void L3::MeshUDPProcessor::_addUCB(std::shared_ptr<MeshUCB> ucb, ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_ucbMapMutex);

        _ucbMap->put(key, ucb);
    }

    _L3->onControlBlockCreated(ucb);
}

void L3::MeshUDPProcessor::_removeUCB(ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_ucbMapMutex);

        _ucbMap->remove(key);
    }
    _L3->releasePort(key.srcAddress().port());

    _L3->onControlBlockDestroyed(key);
}

std::shared_ptr<MeshUCB> L3::MeshUDPProcessor::_findUCB(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_ucbMapMutex);

    if (_ucbMap->exists(key))
        return _ucbMap->get(key);

    return nullptr;
}

void L3::MeshUDPProcessor::_clearAllUCB() {
    Anyfi::Config::lock_type guard(_ucbMapMutex);

    _ucbMap->clear();
}

std::unordered_map<ControlBlockKey, std::shared_ptr<MeshUCB>, ControlBlockKey::Hasher>
L3::MeshUDPProcessor::_copyUCBMap() {
    Anyfi::Config::lock_type guard(_ucbMapMutex);

    auto map = std::unordered_map<ControlBlockKey, std::shared_ptr<MeshUCB>, ControlBlockKey::Hasher>();
    auto it = _ucbMap->listIterBegin();
    while (it != _ucbMap->listIterEnd()) {
        auto cache = *it++;
        auto key = cache.first;
        auto ucb = cache.second;
        map[key] = ucb;
    }

    return map;
}

void L3::MeshUDPProcessor::_timerTick() {
    auto ucbMap = _copyUCBMap();

    for (auto &item: ucbMap) {
        auto ucb = item.second;
        ucb->addExpireTimerTicks();

        auto expiredTick = MESH_UCB_EXPIRE_TIME / MESH_UCB_EXPIRE_TIMER_PERIOD;
        if (ucb->expireTimerTicks() >= expiredTick) {
            // Expired
            _removeUCB(ucb->key());

            _L3->onSessionClosed(ucb, true);
        }
    }
}
