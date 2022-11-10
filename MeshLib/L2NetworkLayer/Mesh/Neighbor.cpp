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

#include "Neighbor.h"
#include "Log/Frontend/Log.h"

void Neighbor::activeTimer(unsigned short idx) {
    Anyfi::Config::lock_type lock(_timeoutMutex);

    MeshTimer::TimeoutConfig timeoutConfig;
    switch (idx) {
        case MeshTimer::HANDSHAKE_TIMER:
            timeoutConfig = MeshTimer::TIMEOUT_HANDSHAKE;
            break;
        case MeshTimer::UPDATEGRAPH_TIMER:
            timeoutConfig = MeshTimer::TIMEOUT_GRAPHUPDATE;
            break;
        case MeshTimer::HEARTBEAT_TIMER:
            timeoutConfig = MeshTimer::TIMEOUT_HEARTBEAT;
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "activating HEARTBEAT_TIMER");
            break;
        default:
            // Invalid idx.
            return;
    }
    _retransmissionCount.at(idx).isEnable = true;
    _retransmissionCount.at(idx).count = timeoutConfig.MAX_TIMEOUT;
    _retransmissionCount.at(idx).period = timeoutConfig.PERIOD;
    _retransmissionCount.at(idx).periodMax = timeoutConfig.PERIOD;
}
void Neighbor::inactiveTimer(unsigned short idx) {
    Anyfi::Config::lock_type lock(_timeoutMutex);

    auto retransCount = &_retransmissionCount.at(idx);
    retransCount->isEnable = false;
    retransCount->count = 0;
    retransCount->period = 0;
    retransCount->periodMax = 0;
}
void Neighbor::inactiveAllTimer() {
    inactiveTimer(MeshTimer::HEARTBEAT_TIMER);
    inactiveTimer(MeshTimer::UPDATEGRAPH_TIMER);
    inactiveTimer(MeshTimer::HANDSHAKE_TIMER);
}
void Neighbor::updateTimer(unsigned short period) {
    Anyfi::Config::lock_type lock(_timeoutMutex);

    for (int i = 0; i < _retransmissionCount.size(); ++i) {
        if (_retransmissionCount[i].isEnable) {
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "updating HEARTBEAT_TIMER");

            _retransmissionCount[i].period -= period;
            if (_retransmissionCount[i].period <= 0 && _channelInfo != nullptr) {
                if (_retransmissionCount[i].count <= 0) {
                    _retransmissionCount[i].isEnable = false;
                    _retransmissionCount[i].count = 0;
                    _retransmissionCount[i].period = 0;
                    _retransmissionFail(_channelInfo->channelId, i);
                }else {
                    _retransmissionCount[i].period = _retransmissionCount[i].periodMax;
                    _timeout(_channelInfo->channelId, i);
                }
                _retransmissionCount[i].count -= 1;
            }
        }
    }
}


void Neighbor::write(NetworkBufferPointer buffer) {
    if (_status != Status::Closed) {
        if (_write)
            _write(buffer);
        else {
            Anyfi::Log::error(__func__, "_write is null");
        }
    }else {
        // ignore
    }
}
void Neighbor::write(std::shared_ptr<MeshPacket> mp) {
    auto buffer = mp->toPacket();
    write(buffer);
}
void Neighbor::writeGraphUpdate(Anyfi::UUID sourceUid, const std::shared_ptr<GraphInfo> &graphInfo,
                                const std::shared_ptr<MeshEdge> &meshEdge, uint64_t updateTime) {
    auto graphUpdateReq = std::make_shared<MeshUpdatePacket>(MeshPacket::Type::GraphUpdateReq, sourceUid, _uid, graphInfo, meshEdge, updateTime);
    auto buffer = graphUpdateReq->toPacket();
    _graphUpdatePacketMap[updateTime] = graphUpdateReq;

    if (_status == Status::Established) {
        _write(buffer);
        activeTimer(MeshTimer::UPDATEGRAPH_TIMER);
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Update: write - " + graphUpdateReq->toString());
    }
}
void Neighbor::sendGraphUpdatePacketInMap() {
    if (!_graphUpdatePacketMap.empty()) {
        if (_status == Status::Established) {
            for(const auto &packet : _graphUpdatePacketMap) {
                auto mup = packet.second;
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Update: rexmt - " + mup->toString());
                _write(mup->toPacket());
            }
            if (!_retransmissionCount.at(MeshTimer::UPDATEGRAPH_TIMER).isEnable) {
                activeTimer(MeshTimer::UPDATEGRAPH_TIMER);
            }
        }
    }
}
void Neighbor::receivedGraphUpdatePacketRes(uint64_t updateTime) {
    auto updatePacketMapIterator = _graphUpdatePacketMap.find(updateTime);
    if (updatePacketMapIterator != _graphUpdatePacketMap.end()) {
        _graphUpdatePacketMap.erase(updatePacketMapIterator);
    }
    if (_graphUpdatePacketMap.empty()) {
        inactiveTimer(MeshTimer::UPDATEGRAPH_TIMER);
    }else {
        // received res. but _graphUpdatePacketMap is not null!!
    }
}

void Neighbor::close(bool isForce) {
    if (_status == Status::Closed) {
        std::cerr << "Neighbor " << uid().toString() << " called " << __func__ << ", but already closed." << std::endl;
        return;
    }
    _status = Status::Closed;
    for (auto &retransCount : _retransmissionCount) {
        retransCount.isEnable = false;
        retransCount.count = 0;
    }
    _graphInfo->nodeList().clear();
    _graphInfo->edgeList().clear();
    if (_close != nullptr && _channelInfo != nullptr) {
        _close(_channelInfo->channelId, isForce);
    }
}
