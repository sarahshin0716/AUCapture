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

#include <functional>
#include <algorithm>

#include "Core.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "MeshNetwork.h"
#include "Log/Frontend/Log.h"
#include "Log/Backend/LogManager.h"


L2::MeshNetwork::MeshNetwork(Anyfi::UUID myUid, const std::string &myDeviceName) {
    _myUid = myUid;
    _myDeviceName = myDeviceName;
    _meshGraph = std::make_shared<MeshGraph>(myUid, myDeviceName);
    _neighborMap.clear();
    _readBufferMap.clear();
    _isTimerCancel = false;
    _logManager = Anyfi::LogManager::getInstance();
}

bool L2::MeshNetwork::isContained(unsigned short channelId) {
    Anyfi::Config::lock_type guard(_neighborMapMutex);
    return _neighborMap.find(channelId) != _neighborMap.end();
}

void L2::MeshNetwork::onRead(unsigned short channelId, const std::shared_ptr<Link> &link, NetworkBufferPointer newBuffer) {
    Anyfi::LogManager::getInstance()->countMeshReadBuffer(newBuffer->getWritePos() - newBuffer->getReadPos());
    // Reassemble
    _readBufferMap[channelId].push_back(newBuffer);
    auto readBufferQueue = &_readBufferMap[channelId];
    bool found = false;
    do {
        // Find readable packet
        found = false;
        auto packetLen = MeshPacket::getPacketLength(readBufferQueue->front());
        std::list<NetworkBufferPointer>::iterator it;
        for(it = readBufferQueue->begin(); it != readBufferQueue->end(); ++it){
            NetworkBufferPointer buffer = *it;
            auto bufferLen = buffer->getWritePos() - buffer->getReadPos();
            if(bufferLen < packetLen) {
                packetLen -= bufferLen;
            } else {
                found = true;
                break;
            }
        }

        if (found){
            NetworkBufferPointer notifyBuf = NetworkBufferPool::acquire();
            packetLen = MeshPacket::getPacketLength(readBufferQueue->front());
            it = readBufferQueue->begin();
            while(it != readBufferQueue->end()){
                NetworkBufferPointer buffer = *it;
                auto bufferLen = buffer->getWritePos() - buffer->getReadPos();
                if(bufferLen <= packetLen) {
                    notifyBuf->putBytes(buffer->buffer() + buffer->getReadPos(), bufferLen);
                    it = readBufferQueue->erase(it);
                    packetLen -= bufferLen;
                } else {
                    notifyBuf->putBytes(buffer->buffer() + buffer->getReadPos(), packetLen);
                    buffer->setReadPos(buffer->getReadPos() + packetLen);
                    break;
                }
            }
            _processRead(channelId, notifyBuf);
        }
    } while(found && !readBufferQueue->empty());
}

void L2::MeshNetwork::onClose(unsigned short channelId) {
    auto neighbor = _getNeighborByLink(channelId);
    if (neighbor != nullptr) {
        Anyfi::Log::error(__func__, "Mesh Network call neighbor close: " + to_string(channelId));
        neighbor->close(false);
    }
}

void L2::MeshNetwork::close(Anyfi::UUID uid) {
    auto neighbor = _getNeighborByUID(uid);
    if (neighbor != nullptr) {
        neighbor->close(true);
    }
}

void L2::MeshNetwork::close(unsigned short channelId) {
    auto neighbor = _getNeighborByLink(channelId);
    if (neighbor != nullptr) {
        neighbor->close(true);
    }
}

void L2::MeshNetwork::onChannelConnected(const ChannelInfo &linkInfo) {
    auto neighbor = _addNeighbor(linkInfo);
    auto mhpReq = std::make_shared<MeshHandshakePacket>(MeshPacket::Type::HandshakeReq, _myUid,
                                                                        Anyfi::UUID(), _meshGraph->getGraphInfo(), 0,
                                                                        _myDeviceName);
    neighbor->status(Neighbor::Status::HandshakeSent);
    neighbor->activeTimer(MeshTimer::HANDSHAKE_TIMER);
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Handshake: write - "+mhpReq->toString());
    neighbor->write(mhpReq->toPacket());
    _logManager->record()->meshHandshakeWrite(MeshPacket::Type::HandshakeReq, mhpReq->toString());
}

void L2::MeshNetwork::onChannelAccepted(const ChannelInfo &channelInfo) {
    Anyfi::LogManager::getInstance()->record()->meshChannelAccepted(channelInfo);
    auto neighbor = _getNeighborByLink(channelInfo.channelId);
    if (neighbor == nullptr) {
        neighbor = _addNeighbor(channelInfo);
    }
    neighbor->status(Neighbor::Status::Accepted);
}

std::shared_ptr<Neighbor> L2::MeshNetwork::_getNeighborByLink(unsigned short channelId) {
    Anyfi::Config::lock_type guard(_neighborMapMutex);

    auto neighborMapIterator = _neighborMap.find(channelId);
    if (neighborMapIterator != _neighborMap.end()) {
        return neighborMapIterator->second;
    } else {
        return nullptr;
    }
}

std::shared_ptr<Neighbor> L2::MeshNetwork::_getNeighborByUID(Anyfi::UUID uid) {
    Anyfi::Config::lock_type guard(_neighborMapMutex);

    auto toFind = [uid](const std::pair<unsigned short, std::shared_ptr<Neighbor>> &p) {
        return p.second->uid() == uid;
    };
    auto neighborMapIterator = std::find_if(_neighborMap.begin(), _neighborMap.end(), toFind);
    if (neighborMapIterator != _neighborMap.end()) {
        return neighborMapIterator->second;
    } else {
        return nullptr;
    }
}

std::shared_ptr<Neighbor> L2::MeshNetwork::_addNeighbor(const ChannelInfo &channelInfo) {
    auto neighbor = std::make_shared<Neighbor>();
    neighbor->channelInfo(channelInfo);
    auto neighborChannelId = channelInfo.channelId;
    neighbor->write([this, neighborChannelId](const NetworkBufferPointer &buffer) {
        this->_L2->writeToChannel(neighborChannelId, buffer);
    });

    neighbor->close(std::bind(&L2::MeshNetwork::_neighborClosed, this, std::placeholders::_1, std::placeholders::_2));
    neighbor->timeout(
            std::bind(&L2::MeshNetwork::_neighborTimeout, this, std::placeholders::_1, std::placeholders::_2));
    neighbor->retransmissionFailed(
            std::bind(&L2::MeshNetwork::_neighborRetransmissionFailed, this, std::placeholders::_1,
                      std::placeholders::_2));

    _addNeighborMap(neighborChannelId, neighbor);

    return neighbor;
}

void L2::MeshNetwork::_closeAllNeighbors() {
    auto neighborMap = _copyNeighborMap();

    for (auto &neighbor : neighborMap) {
        if (neighbor.second != nullptr) {
            neighbor.second->close(false);
        }
    }
}

void L2::MeshNetwork::_processRead(unsigned short channelId, NetworkBufferPointer buffer) {
    auto packetType = MeshPacket::getType(buffer);
    if (packetType == MeshPacket::Type::HandshakeReq || packetType == MeshPacket::Type::HandshakeReqRes ||
        packetType == MeshPacket::Type::HandshakeRes) {
        _onReadMeshHandshakePacket(channelId, MeshHandshakePacket::toSerialize(buffer, 0));
    } else if (packetType == MeshPacket::Type::Datagram) {
        auto datagramPacket = MeshDatagramPacket::toSerialize(buffer);
        auto neighbor = _getNeighborByLink(channelId);
        if (neighbor != nullptr) {
            neighbor->activeTimer(MeshTimer::HEARTBEAT_TIMER);
            if (datagramPacket->destUid() == _myUid) {
                if (neighbor->status() == Neighbor::Status::Established) {
                    Anyfi::Address sourceAddr;
                    sourceAddr.type(Anyfi::AddrType::MESH_UID);
                    sourceAddr.setMeshUID(datagramPacket->sourceUid());
                    sourceAddr.connectionType(neighbor->channelInfo()->addr.connectionType());
                    _L2->onReadMeshPacketFromNetwork(sourceAddr, datagramPacket->payload());
                }
            } else {
                auto fastestEdge = _meshGraph->getFastestEdge(datagramPacket->destUid());
                if (fastestEdge != nullptr) {
                    auto fastestUid = fastestEdge->secondUid();
                    auto fastestNeighbor = _getNeighborByUID(fastestUid);
                    if (fastestNeighbor != nullptr) {
                        // 내게로 온 패킷이 아니므로, 다시 read position을 리셋하고 L1으로 내려보낸다.
                        buffer->setReadPos(0);
                        fastestNeighbor->write(buffer);
                    } else {
                        Anyfi::Log::error(__func__, "L2:: fastestNeighbor is nullptr");
                    }
                } else {
                    Anyfi::Log::error(__func__, "L2:: fastestEdge is nullptr");
                }
            }
        }
    } else if (packetType == MeshPacket::Type::GraphUpdateReq || packetType == MeshPacket::Type::GraphUpdateRes) {
        auto neighbor = _getNeighborByLink(channelId);
        if (neighbor != nullptr && neighbor->status() == Neighbor::Status::Established) {
            neighbor->activeTimer(MeshTimer::HEARTBEAT_TIMER);
            _onMeshUpdatePacket(neighbor, MeshUpdatePacket::toSerialize(buffer));
        }
    } else if (packetType == MeshPacket::Type::HeartbeatReq || packetType == MeshPacket::Type::HeartbeatRes) {
        auto heartbeatPacket = MeshHeartBeatPacket::toSerialize(buffer);
        _onMeshHeartBeatPacket(heartbeatPacket);
    } else {
        Anyfi::Log::error(__func__, "L2:: Invalid Mesh Packet Type");
    }
}

void L2::MeshNetwork::_onReadMeshHandshakePacket(unsigned short channelId,
                                                 std::shared_ptr<MeshHandshakePacket> mhp) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Handshake: read - " + mhp->toString());
    auto neighbor = _getNeighborByLink(channelId);
    if (neighbor == nullptr) {
        Anyfi::Log::error("MESH", "Neighbor does not exist. Received protocol: " + to_string(mhp->type()));
    } else if (neighbor->status() == Neighbor::Status::Accepted) {
        _logManager->record()->meshHandshakeRead(mhp->type(), mhp->toString());
        /*
         * Received first Handshake request Packet.
         * Send Handshake Request/Response Packet containing my information.
         */
        if (mhp->type() == MeshPacket::Type::HandshakeReq) {
            /* Clean states if already exists */
            auto prevNeighbor = _getNeighborByUID(mhp->sourceUid());
            if (prevNeighbor != nullptr) {
                prevNeighbor->inactiveAllTimer();
                clean(prevNeighbor->uid());
                _removeNeighborMap(prevNeighbor->channelInfo()->channelId);
                Anyfi::Address addr;
                addr.type(Anyfi::AddrType::MESH_UID);
                addr.setMeshUID(neighbor->uid());
                _L2->onChannelClosed(channelId, addr);
            }
            /* 해당 uid의 neighbor가 있을 경우 지운 다음, neighbor->uid 세팅 */
            neighbor->status(Neighbor::Status::HandshakeRecv);
            neighbor->uid(mhp->sourceUid());
            neighbor->deviceName(mhp->deviceName());
            neighbor->graph(mhp->graphInfo());

            /* Get This edge's update sequence */
            auto myUpdateTime = _meshGraph->getUpdateTime(_myUid, mhp->sourceUid());
            if (myUpdateTime == 0) {
                myUpdateTime = getCurrentTimeMilliseconds();
            }

            /* Get Old Graph to send */
            auto myGraph = _meshGraph->getGraphInfo();

            /* Get Required Edge Sequences to send */
            auto disconnGraph = _meshGraph->getDisconnGraph(mhp->graphInfo());
            myGraph->addEdge(disconnGraph->edgeList());
            myGraph->addNode(disconnGraph->nodeList());

            /* Write back Mesh Handshake Req Res Protocol */
            auto meshHandshakeReqRes = std::make_shared<MeshHandshakePacket>(MeshPacket::Type::HandshakeReqRes, _myUid,
                                                                             mhp->sourceUid(), myGraph, myUpdateTime,
                                                                             _myDeviceName);
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Handshake: write - " + meshHandshakeReqRes->toString());
            neighbor->activeTimer(MeshTimer::HANDSHAKE_TIMER);
            neighbor->write(meshHandshakeReqRes->toPacket());
        } else {
            // ignore
        }
    } else {
        switch (neighbor->status()) {
            case Neighbor::Status::HandshakeSent:
                switch (mhp->type()) {
                    case MeshPacket::Type::HandshakeReq:
                    case MeshPacket::Type::HandshakeRes: {
                        Anyfi::Log::error("MESH", "Mesh Handshake Invalid packet received (type:" +
                                                  MeshPacket::toString(mhp->type()) + ")");
                        break;
                    }
                    case MeshPacket::Type::HandshakeReqRes: {
                        neighbor->status(Neighbor::Status::Established);
                        neighbor->uid(mhp->sourceUid());
                        neighbor->deviceName(mhp->deviceName());

                        /* Cancel Handshake Retransmission timer */
                        neighbor->inactiveTimer(MeshTimer::HANDSHAKE_TIMER);

                        /* Set Update Sequence */
                        uint64_t myUpdateTime = _meshGraph->getUpdateTime(_myUid, mhp->sourceUid());
                        if (myUpdateTime == 0) {
                            myUpdateTime = getCurrentTimeMilliseconds();
                        }
                        myUpdateTime = std::max(myUpdateTime, mhp->updateTime());

                        /* Get Disconn Graph */
                        auto disconnGraph = _meshGraph->getDisconnGraph(mhp->graphInfo());

                        /* Add Grpah and Remove DISCONN Graph */
                        auto newGraph = mhp->graphInfo();
                        auto connectedEdge = std::make_shared<MeshEdge>(_myUid, mhp->sourceUid(),
                                                                        myUpdateTime + 1, 1);

                        auto myMeshNode = std::make_shared<MeshNode>(_myUid,
                                                                     Anyfi::Core::getInstance()->myDeviceName(),
                                                                     _myMeshNodeType);
                        newGraph->addEdge(connectedEdge);
                        newGraph->addNode(myMeshNode);
                        auto prevNodes = _meshGraph->getConnectedNodes();
                        auto changes = _meshGraph->updateGraph(newGraph);

                        // 연결 경로가 사라진 노드 close 처리
                        auto removedNodes = _meshGraph->getRemovedNodes(prevNodes);
                        for (auto &removedUid : removedNodes) {
                            auto removedNeighbor = _getNeighborByUID(removedUid);
                            if (removedNeighbor != nullptr &&
                                removedNeighbor->channelInfo() != nullptr) {
                                auto addr = removedNeighbor->channelInfo()->addr;
                                neighbor->close(false);
                            }
                        }

                        /* Write Res */
                        auto meshHandshakeRes = std::make_shared<MeshHandshakePacket>(
                                MeshPacket::Type::HandshakeRes,
                                _myUid, mhp->sourceUid(),
                                disconnGraph, myUpdateTime,
                                _myDeviceName);
                        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__,
                                          "Mesh Handshake: write - " +
                                          meshHandshakeRes->toString());
                        neighbor->write(meshHandshakeRes->toPacket());
                        // TODO: graph 변화정보 application에 전달

                        /* Broadcast updated graph */
                        _broadcastGraph(mhp->sourceUid(), newGraph, connectedEdge);

                        /* Send unsent update protocol */
                        neighbor->sendGraphUpdatePacketInMap();


                        _onNeighborEstablished(neighbor);
                        break;
                    }
                    default:
                        break;
                }
                break;
            case Neighbor::Status::HandshakeRecv:
                switch (mhp->type()) {
                    case MeshPacket::Type::HandshakeReq: {
                        /* Get This edge's update sequence */
                        auto myUpdateTime = _meshGraph->getUpdateTime(_myUid, mhp->sourceUid());
                        if (myUpdateTime == 0) {
                            myUpdateTime = getCurrentTimeMilliseconds();
                        }
                        /* Get Old Graph to send */
                        auto myGraph = _meshGraph->getGraphInfo();

                        /* Get Required Edge Sequences to send */
                        auto disconnGraph = _meshGraph->getDisconnGraph(mhp->graphInfo());
                        disconnGraph->addEdge(myGraph->edgeList());
                        disconnGraph->addNode(myGraph->nodeList());

                        /* Ignore duplicates */
                        auto mhpReqRes = std::make_shared<MeshHandshakePacket>(MeshPacket::Type::HandshakeReqRes,
                                _myUid, mhp->sourceUid(), myGraph, myUpdateTime, _myDeviceName);
                        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Handshake: write - " + mhpReqRes->toString());

                        neighbor->activeTimer(MeshTimer::HANDSHAKE_TIMER);
                        neighbor->write(mhpReqRes->toPacket());
                        break;
                    }
                    case MeshPacket::Type::HandshakeRes: {
                        neighbor->status(Neighbor::Status::Established);

                        /* Cancel Retransmission timer */
                        neighbor->inactiveTimer(MeshTimer::HANDSHAKE_TIMER);

                        /* Add Grpah and Remove DISCONN Graph */
                        auto newGraph = neighbor->graph();

                        newGraph->addEdge(mhp->graphInfo()->edgeList());
                        newGraph->addNode(mhp->graphInfo()->nodeList());

                        auto connectedEdge = std::make_shared<MeshEdge>(_myUid, mhp->sourceUid(), mhp->updateTime() + 1, 1);
                        auto myMeshNode = std::make_shared<MeshNode>(_myUid, Anyfi::Core::getInstance()->myDeviceName(), _myMeshNodeType);

                        newGraph->addEdge(connectedEdge);
                        newGraph->addNode(myMeshNode);

                        auto prevNodes = _meshGraph->getConnectedNodes();
                        auto changes = _meshGraph->updateGraph(newGraph);

                        /* Clean removed nodes' memory */
                        auto removedNodes = _meshGraph->getRemovedNodes(prevNodes);
                        for (auto &removedUid : removedNodes) {
                            auto removedNeighbor = _getNeighborByUID(removedUid);
                            if (removedNeighbor != nullptr) {
                                auto addr = removedNeighbor->channelInfo()->addr;
                                neighbor->close(false);
                            }
                        }

                        /* Broadcast updated graph */
                        _broadcastGraph(mhp->sourceUid(), newGraph, connectedEdge);

                        /* Send unsent update protocol */
                        neighbor->sendGraphUpdatePacketInMap();


                        _onNeighborEstablished(neighbor);
                        break;
                    }
                    case MeshPacket::Type::HandshakeReqRes: {
                        Anyfi::Log::error("MESH", "Mesh Handshake Invalid protocol received");
                        break;
                    }
                    default:
                        break;
                }
                break;
            case Neighbor::Status::Established:
                switch (mhp->type()) {
                    /**
                     * 자기 자신의 디바이스는 REQRES를 처리하고 Established가 됐지만, RES가 분실되어 REQRES가 다시 들어온 케이스.
                     * RES를 다시 만들어 전송한다.
                     */
                    case MeshPacket::Type::HandshakeReqRes: {
                        auto disconnGraph = _meshGraph->getDisconnGraph(mhp->graphInfo());
                        uint64_t myUpdateTime = _meshGraph->getUpdateTime(_myUid, mhp->sourceUid());
                        if (myUpdateTime != 0) {
                            /* Write back Mesh Handshake Res */
                            auto mhpRes = std::make_shared<MeshHandshakePacket>(
                                    MeshPacket::Type::HandshakeRes, _myUid, mhp->sourceUid(),
                                    disconnGraph,
                                    myUpdateTime - 1, _myDeviceName);
                            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__,
                                              "Mesh Handshake: write - " + mhpRes->toString());
                            neighbor->write(mhpRes->toPacket());
                            /* Send unsent update protocol */
                            neighbor->sendGraphUpdatePacketInMap();
                        } else {
                            Anyfi::Log::error("MESH",
                                              "NEIGHBOR STATE ESTABLISHED but update time null");
                        }
                        break;
                    }
                    default:
                        break;
                }
                break;
            case Neighbor::Status::Closed:
                // Ignore
                break;
            case Neighbor::Status::Accepted:
                break;
        } // switch (neighbor->status())

    } // if (neighbor != nullptr)
}

void L2::MeshNetwork::_onMeshUpdatePacket(std::shared_ptr<Neighbor> neighbor, std::shared_ptr<MeshUpdatePacket> mup) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Update: read - " + mup->toString());
    std::string typeString = (mup->type() == MeshPacket::Type::GraphUpdateReq) ? "Req" : "res";
    if (neighbor != nullptr && mup->destUid() == _myUid && mup->sourceUid() == neighbor->uid()) {
        if (mup->type() == MeshPacket::Type::GraphUpdateReq) {
            /* Broadcast if updated */
            auto currentUpdateTime = _meshGraph->getUpdateTime(mup->meshEdge());

            auto meshEdge = mup->meshEdge();
            if (meshEdge->firstUid() != Anyfi::UUID() && meshEdge->secondUid() != Anyfi::UUID()) {
                if (meshEdge->updateTime() > currentUpdateTime) {
                    auto prevNodes = _meshGraph->getConnectedNodes();
                    auto changes = _meshGraph->updateGraph(mup->graphInfo());
                    /* Clean removed node's memory */
                    auto removedNodes = _meshGraph->getRemovedNodes(prevNodes);
                    std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses;
                    for (auto &removedUid : removedNodes) {
                        auto removedNeighbor = _getNeighborByUID(removedUid);
                        if (removedNeighbor != nullptr) {
                            auto addr = removedNeighbor->channelInfo()->addr;
                            neighbor->close(false);
                        } else {
                            Anyfi::Address addr;
                            addr.type(Anyfi::AddrType::MESH_UID);
                            addr.setMeshUID(removedUid);
                            addresses.insert(addr);
                        }
                    }
                    if (addresses.size() > 0) {
                        _L2->onMeshNodeClosed(addresses);
                    }
                    _broadcastGraph(mup->sourceUid(), mup->graphInfo(), mup->meshEdge());
                }
            } else {
                // MeshEdge가 비어있을 경우, NodeList만 비교 및 적용
                auto nodeList = mup->graphInfo()->nodeList();
                int updateCount = _meshGraph->updateNodeList(nodeList);
                if (updateCount > 0) {
                    _broadcastGraph(mup->sourceUid(), mup->graphInfo(), mup->meshEdge());
                }
            }
            /* Write ack */
            auto mupRes = std::make_shared<MeshUpdatePacket>(MeshPacket::Type::GraphUpdateRes, _myUid, mup->sourceUid(),
                                                             nullptr, mup->meshEdge(), mup->updateTime());
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Update: write Ack - " + mupRes->toString());
            neighbor->write(mupRes->toPacket());

            Anyfi::LogManager::getInstance()->record()->meshGraphUpdateRead(mup->sourceUid(), _meshGraph->getGraphInfo()->toString());

        } else if (mup->type() == MeshPacket::Type::GraphUpdateRes) {
            neighbor->receivedGraphUpdatePacketRes(mup->updateTime());
        } else {
            Anyfi::Log::error("MESH", "Invalid Mesh udpate packet type");
        }
    } else {
        // ignore.
    }
}

void L2::MeshNetwork::_onMeshHeartBeatPacket(std::shared_ptr<MeshHeartBeatPacket> heartbeatPacket) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Heartbeat: read - " + heartbeatPacket->toString());
    auto neighbor = _getNeighborByUID(heartbeatPacket->sourceUid());
    if (neighbor != nullptr && neighbor->status() == Neighbor::Status::Established) {
        neighbor->activeTimer(MeshTimer::HEARTBEAT_TIMER);
        if (heartbeatPacket->type() == MeshPacket::Type::HeartbeatReq) {
            auto heartBeatRes = std::make_shared<MeshHeartBeatPacket>(MeshPacket::Type::HeartbeatRes, _myUid,
                                                                      neighbor->uid());
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Heartbeat: write - " + heartBeatRes->toString());
            neighbor->write(heartBeatRes->toPacket());
        }else if (heartbeatPacket->type() == MeshPacket::Type::HeartbeatRes) {
        }
    }
}

void L2::MeshNetwork::_neighborClosed(unsigned short channelId, bool isForce) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh State: Neighbor Closed - channelId: "+to_string(channelId)+", "+std::string(isForce?"true":"false")+"");
    auto closeNeighbor = _getNeighborByLink(channelId);
    _removeNeighborMap(channelId);
    auto uid = closeNeighbor->uid();
    clean(uid);
    Anyfi::Address meshAddr;
    meshAddr.type(Anyfi::AddrType::MESH_UID);
    meshAddr.setMeshUID(uid);
    meshAddr.connectionType(Anyfi::ConnectionType::WifiP2P); // TODO: Apply general type
    if (isForce) {
        Anyfi::Log::error(__func__, "Mesh Network call L2 closeChannel2: " + to_string(channelId));
        _L2->closeNeighborChannel(channelId);
    } else {
        Anyfi::Log::error(__func__, "Mesh Network call L2 onChannelClosed2: " + to_string(channelId));
        _L2->onChannelClosed(channelId, meshAddr);
    }
}

void L2::MeshNetwork::_neighborTimeout(unsigned short channelId, unsigned short timerIdx) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh State: Neighbor Timeout - channelId: "+to_string(channelId)+", timerIdx: "+to_string(timerIdx));
    auto timeoutNeighbor = _getNeighborByLink(channelId);
    if (timeoutNeighbor)
        _retransmit(timeoutNeighbor, timerIdx);
}

void L2::MeshNetwork::_neighborRetransmissionFailed(unsigned short channelId, unsigned short timerIdx) {
    /**
     * Retransmission all failed.
     * Status만 Closed로 바꾸고 연결 해제 요청을 하고, 실질적인 정리는 해제가 되었을 때 한다.
     */
    auto failedNeighbor = _getNeighborByLink(channelId);
    if (failedNeighbor == nullptr)
        return;

    Anyfi::Log::error("MESH", "NEIGHBOR RETRANSMISSION FAILED: " + failedNeighbor->uid().toString() + ", TIMER IDX: " + to_string(timerIdx));
    _logManager->record()->meshRetransmissionFailed(failedNeighbor->uid(), timerIdx);

    _removeNeighborMap(channelId);
    clean(failedNeighbor->uid());
    if (timerIdx == MeshTimer::HANDSHAKE_TIMER) {
        _L2->onMeshConnectFail(failedNeighbor->channelInfo());
    } else {
        failedNeighbor->status(Neighbor::Status::Closed);
        _L2->closeNeighborChannel(channelId);
        Anyfi::Address addr = Anyfi::Address();
        addr.connectionType(Anyfi::ConnectionType::WifiP2P);    // TODO: Support other P2P Type
        addr.type(Anyfi::AddrType::MESH_UID);
        addr.setMeshUID(failedNeighbor->uid());
        _L2->onChannelClosed(channelId, addr);
    }
}

void L2::MeshNetwork::_onNeighborEstablished(std::shared_ptr<Neighbor> neighbor) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh State: Neighbor Established - channelId: " + to_string(neighbor->channelInfo()->channelId));
    Anyfi::LogManager::getInstance()->record()->meshNeighborEstablished(neighbor->uid(), _meshGraph->getGraphInfo()->toString());
    // active heartbeat timer
    neighbor->activeTimer(MeshTimer::HEARTBEAT_TIMER);
    _L2->onMeshConnected(neighbor->channelInfo(), neighbor->uid());
}

bool L2::MeshNetwork::write(Anyfi::UUID dst_uid, NetworkBufferPointer buffer) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    Anyfi::LogManager::getInstance()->countMeshWriteBuffer(buffer->getWritePos() - buffer->getReadPos());

    auto fastestEdge = _meshGraph->getFastestEdge(dst_uid);
    if (fastestEdge != nullptr) {
        auto neighbor = _getNeighborByUID(fastestEdge->secondUid());
        if (neighbor != nullptr && neighbor->status() == Neighbor::Status::Established) {
            auto datagramPacket = std::make_shared<MeshDatagramPacket>(_myUid, dst_uid);
            datagramPacket->toPacket(buffer);
//            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Datagram: write - " + datagramPacket->toString());
            neighbor->write(buffer);
            return true;
        }
    }else {
        Anyfi::Log::error(__func__, "L2:: could not find fastest edge (dest: "+dst_uid.toString()+")" );
    }
    return false;
}

void L2::MeshNetwork::_write(Anyfi::UUID dst_uid, std::shared_ptr<MeshPacket> meshProtocol) {

}

void L2::MeshNetwork::_broadcastGraph(Anyfi::UUID sourceUid, std::shared_ptr<GraphInfo> graphInfo,
                                      std::shared_ptr<MeshEdge> meshEdge) {
    auto neighborMap = _copyNeighborMap();
    for (const auto &neighborIt : neighborMap) {
        Anyfi::UUID destUid = neighborIt.second->uid();
        if (neighborIt.second != nullptr && destUid != Anyfi::UUID() && destUid != sourceUid) {
            if (neighborIt.second->status() == Neighbor::Status::Established ||
                neighborIt.second->status() == Neighbor::Status::HandshakeRecv ||
                neighborIt.second->status() == Neighbor::Status::HandshakeSent) {
                uint64_t updateTime = 0;
                if (meshEdge)
                    updateTime = meshEdge->updateTime();
                else
                    updateTime = getCurrentTimeMilliseconds();
                neighborIt.second->writeGraphUpdate(_myUid, graphInfo, meshEdge, updateTime);
                // if ((Log.Debug.LogSetting & Log.Debug.Mesh_Graph) > 0) {
                //  Log.i(Log.Debug.MeshTag, "MESH WRITE UPDATE: " + ((MeshUpdateProtocol) mup).toString());
                // }
            }
        }
    }
}

void L2::MeshNetwork::_retransmit(std::shared_ptr<Neighbor> neighbor, unsigned short timerIdx) {
    switch (timerIdx) {
        case MeshTimer::HANDSHAKE_TIMER:
            // Handshake retransmit
            switch (neighbor->status()) {
                case Neighbor::Status::HandshakeSent: {
                    /* Retransmit Mesh Handshake Request packet */
                    auto graphInfo = _meshGraph->getGraphInfo();
                    auto mhp = std::make_shared<MeshHandshakePacket>(MeshPacket::Type::HandshakeReq, _myUid,
                                                                     neighbor->uid(), graphInfo, 0, _myDeviceName);

                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Handshake: rexmt - " + mhp->toString());
                    neighbor->write(mhp->toPacket());
                    _logManager->record()->meshRetransmissionWrite(mhp->type());
                    break;
                }
                case Neighbor::Status::HandshakeRecv: {
                    /* Get This edge's update sequence */
                    auto myUpdateTime = _meshGraph->getUpdateTime(_myUid, neighbor->uid());
                    if (myUpdateTime == 0) {
                        myUpdateTime = getCurrentTimeMilliseconds();
                    }
                    /* Get Old Graph to send */
                    auto myGraph = _meshGraph->getGraphInfo();

                    /*Get Required Edge Sequences to send */
                    auto disconnGraph = _meshGraph->getDisconnGraph(neighbor->graph());
                    myGraph->addEdge(disconnGraph->edgeList());
                    myGraph->addNode(disconnGraph->nodeList());

                    /* Retransmit Mesh Handshake Req/Res packet */
                    auto mhp = std::make_shared<MeshHandshakePacket>(MeshPacket::Type::HandshakeReqRes, _myUid,
                                                                     neighbor->uid(), myGraph, myUpdateTime,
                                                                     _myDeviceName);

                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Handshake: rexmt - " + mhp->toString());
                    neighbor->write(mhp->toPacket());
                    _logManager->record()->meshRetransmissionWrite(mhp->type());
                    break;
                }
                case Neighbor::Status::Closed:
                    break;
                case Neighbor::Status::Established:
                    break;
                case Neighbor::Status::Accepted:
                    break;
            }
            break;
        case MeshTimer::UPDATEGRAPH_TIMER: {
            if (neighbor->status() == Neighbor::Status::Established) {
                neighbor->sendGraphUpdatePacketInMap();
                _logManager->record()->meshRetransmissionWrite(MeshPacket::Type::GraphUpdateReq);
            }
            break;
        }
        case MeshTimer::HEARTBEAT_TIMER: {
            if (neighbor->status() == Neighbor::Status::Established) {
                std::shared_ptr<MeshPacket> heartbeatPacket = std::make_shared<MeshHeartBeatPacket>(MeshPacket::Type::HeartbeatReq, _myUid,
                                                                             neighbor->uid());
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh Heartbeat: rexmt - ");
                Anyfi::Core::getInstance()->enqueueTask([neighbor, heartbeatPacket] {
                    neighbor->write(std::dynamic_pointer_cast<MeshPacket>(heartbeatPacket));
                });
            }
            break;
        }
        default:
            Anyfi::Log::error("MESH", "Invalid timerIdx");
            break;
    }
}

void L2::MeshNetwork::timeout() {
    //Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2, "L2::MeshNetwork::timeout", "threadId=" + thread_id_str());

    std::stringstream ss;
    ss << "Meshtimeout";
    pthread_setname_np(pthread_self(), ss.str().c_str());

    auto timeoutBase = MeshTimer::TIMEOUT_BASE;
    while (!_isTimerCancel) {
        std::unordered_map<unsigned short, std::shared_ptr<Neighbor>> neighborMap = _neighborMap;
        while (!neighborMap.empty()) {
            auto neighbor = *neighborMap.begin();
            neighborMap.erase(neighborMap.begin());
            if (neighbor.second->status() != Neighbor::Status::Closed &&
                neighbor.second->status() != Neighbor::Status::Accepted) {
                neighbor.second->updateTimer(timeoutBase);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutBase));
    }
}

std::unordered_set<Anyfi::UUID, Anyfi::UUID::Hasher> L2::MeshNetwork::clean(Anyfi::UUID uid) {
    std::unordered_set<Anyfi::UUID, Anyfi::UUID::Hasher> removedNodes;
    /* Broadcast closed link. Skip if does not exist in graph */
    auto closedEdge = _meshGraph->getNeighborEdge(uid);
    if (closedEdge != nullptr) {
        closedEdge->cost(-1);
        auto closedEdgeUpdateTime = _meshGraph->getUpdateTime(_myUid, uid);
        if (closedEdgeUpdateTime != 0) {
            closedEdge->updateTime(closedEdgeUpdateTime + 1);
        } else {
            // TODO: print log
        }
        auto closedGraph = std::make_shared<GraphInfo>();
        closedGraph->addEdge(closedEdge);

        auto prevNodes = _meshGraph->getConnectedNodes();
        auto chages = _meshGraph->updateGraph(closedGraph);
        removedNodes = _meshGraph->getRemovedNodes(prevNodes);

        auto uidit = removedNodes.find(uid);
        if (uidit != removedNodes.end()) {
            removedNodes.erase(uidit);
        }
        if (!removedNodes.empty()) {
            std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses;
            for (const auto &uuid : removedNodes) {
                Anyfi::Address addr;
                addr.type(Anyfi::AddrType::MESH_UID);
                addr.setMeshUID(uuid);
                addresses.insert(addr);
            }
            _L2->onMeshNodeClosed(addresses);
        }

        _broadcastGraph(uid, closedGraph, closedEdge);

    }
    return removedNodes;
}

void L2::MeshNetwork::setMeshNodeType(uint8_t type) {
    _myMeshNodeType = type;
    auto meshNode = _meshGraph->setMeshNodeType(type);
    auto graphInfo = std::make_shared<GraphInfo>();
    graphInfo->addNode(meshNode);
    _broadcastGraph(_myUid, graphInfo, nullptr);
}

std::pair<Anyfi::Address, uint8_t> L2::MeshNetwork::getClosestMeshNode(uint8_t type) {
    return _meshGraph->getClosestMeshNode(type);
}

void L2::MeshNetwork::clean() {
    cancelTimer();
}

void L2::MeshNetwork::initTimer() {
    _timeoutThread = std::unique_ptr<std::thread>(new std::thread(&L2::MeshNetwork::timeout, this));
}

void L2::MeshNetwork::cancelTimer() {
    if (_timeoutThread != nullptr && _timeoutThread->joinable()) {
        _isTimerCancel = true;
        _timeoutThread->join();
        _timeoutThread.reset();
    }
}

void L2::MeshNetwork::_addNeighborMap(unsigned short channelId, const std::shared_ptr<Neighbor> &neighbor) {
    Anyfi::Config::lock_type guard(_neighborMapMutex);
    _neighborMap[channelId] = neighbor;
}

void L2::MeshNetwork::_removeNeighborMap(unsigned short channelId) {
    Anyfi::Config::lock_type guard(_neighborMapMutex);
    auto neighborIterator = _neighborMap.find(channelId);
    if (neighborIterator != _neighborMap.end()) {
        _neighborMap.erase(neighborIterator);
    }
}

std::unordered_map<unsigned short, std::shared_ptr<Neighbor>> L2::MeshNetwork::_copyNeighborMap() {
    Anyfi::Config::lock_type guard(_neighborMapMutex);
    auto neighborMap = _neighborMap;
    return neighborMap;
}



