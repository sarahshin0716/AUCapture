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

#include <memory>

#include "Log/Frontend/Log.h"
#include "L4AnyfiLayer/L4AnyfiLayer.h"
#include "AnyfiBridgeApplication.h"
#include "AnyfiBridgePacket.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "../../../Common/Network/Address.h"

void L4::AnyfiBridgeApplication::onSessionConnectFromVPN(unsigned short vpnCbId, std::vector<Anyfi::Address> gatewayList, bool useMain, Anyfi::Address srcAddr, Anyfi::Address destAddr) {
    if (_bridgeMap.get(vpnCbId) != nullptr) {
        throw std::runtime_error("duplicated control block: " + to_string(vpnCbId));
    }

    // Construct bridge List
    auto bridgeList = std::make_shared<std::vector<std::shared_ptr<AnyfiBridge>>>();
    for (auto gwAddr : gatewayList) {
        auto newBridge = std::make_shared<AnyfiBridge>(_L4);
        newBridge->vpnCbId(vpnCbId);
        newBridge->srcAddr(srcAddr);

        Anyfi::Address address = Anyfi::Address(destAddr.type(), destAddr.connectionType(),
                                                destAddr.connectionProtocol(), destAddr.addr(),
                                                destAddr.port());
        address.networkType(gwAddr.networkType());

        if (address.networkType() == Anyfi::NetworkType::WIFI || address.networkType() == Anyfi::NetworkType::MOBILE || address.networkType() == Anyfi::NetworkType::DEFAULT) {
            newBridge->type(AnyfiBridge::Type::VpnToProxy);
            newBridge->connectAddr(address);
        } else if (address.networkType() == Anyfi::NetworkType::P2P) {
            newBridge->type(AnyfiBridge::Type::VpnToMesh);

            if (destAddr.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
                gwAddr.connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);
            }else if (destAddr.connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
                gwAddr.connectionProtocol(Anyfi::ConnectionProtocol::MeshUDP);
            }
            newBridge->connectAddr(gwAddr);
        }

        bridgeList->insert(bridgeList->end(), newBridge);
    }
    if (useMain) {
        bridgeList->front()->setMain(true);
    }

    _bridgeMap.addBridge(vpnCbId, bridgeList);

    // Connect bridges
    auto bridgeVector = *bridgeList;    // Copy to handle remove while iterating
    for (auto bridge : bridgeVector) {
#ifdef NDEBUG
#else
        if (bridge->connectAddr().networkType() == Anyfi::NetworkType::WIFI) {
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect(Wi-Fi) to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()) + (bridge->isMain() ? std::string(" - Main") : ""));
        } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::P2P) {
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect(P2P) to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()) + (bridge->isMain() ? std::string(" - Main") : ""));
        } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::MOBILE) {
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect(Mobile) to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()) + (bridge->isMain() ? std::string(" - Main") : ""));
        } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::DEFAULT) {
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()) + (bridge->isMain() ? std::string(" - Main") : ""));
        }
#endif

        bridge->connect(bridge->connectAddr(), [this, bridge, destAddr](unsigned short newCbId) {
            if (newCbId == 0) {
#ifdef NDEBUG
#else
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connect failed for VPN cbId: " + to_string(bridge->vpnCbId()));
#endif
                _handleSessionClosedByDest(bridge, false);
            } else if (bridge->vpnCbId() == 0) {
#ifdef NDEBUG
#else
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected, but VPN link disconnected for cbId: " + to_string(newCbId));
#endif
                bridge->close(newCbId, false);
            } else {
                if (bridge->isMain() || !_hasMain(bridge->vpnCbId())) {
                    // Connected. Add other side's bridge
#ifdef NDEBUG
#else
                    if (bridge->type() == AnyfiBridge::Type::VpnToProxy) {
                        if (bridge->connectAddr().networkType() == Anyfi::NetworkType::WIFI) {
                            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected proxy (Wi-Fi): " + to_string(bridge->vpnCbId()) + " -> " + to_string(bridge->proxyCbId()) + std::string(bridge->isMain() ? "" : " - Set Main"));
                        } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::MOBILE) {
                            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected proxy (Mobile): " + to_string(bridge->vpnCbId()) + " -> " + to_string(bridge->proxyCbId()) + std::string(bridge->isMain() ? "" : " - Set Main"));
                        } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::DEFAULT) {
                            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected proxy: " + to_string(bridge->vpnCbId()) + " -> " + to_string(bridge->proxyCbId()) + std::string(bridge->isMain() ? "" : " - Set Main"));
                        }
                    } else if (bridge->type() == AnyfiBridge::Type::VpnToMesh) {
                        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected mesh (P2P): " + to_string(bridge->vpnCbId()) + " -> " + to_string(bridge->proxyCbId()) + std::string(bridge->isMain() ? "" : " - Set Main"));
                    }
#endif
                    auto singleBridgeList = std::make_shared<std::vector<std::shared_ptr<AnyfiBridge>>>();
                    singleBridgeList->insert(singleBridgeList->end(), bridge);
                    _bridgeMap.addBridge(newCbId, singleBridgeList);

                    // Set main if not a main and close others
                    if (!bridge->isMain()) {
                        bridge->setMain(true);
                        _closeNonMainBridges(bridge->vpnCbId(), true);
                    }

                    // If Mesh connection, send AnyfiBridgeConnPacket
                    if (bridge->type() == AnyfiBridge::Type::VpnToMesh) {
                        std::shared_ptr<AnyfiBridgeConnPacket> connPacket = std::make_shared<AnyfiBridgeConnPacket>(destAddr);
                        NetworkBufferPointer wrappedBuffer = connPacket->toPacket();
#ifdef NDEBUG
#else
                        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "write vpn to mesh " + to_string(bridge->vpnCbId()) + " -> " + to_string(bridge->meshCbId()) + " (buffer size: " + to_string(wrappedBuffer->getWritePos() - wrappedBuffer->getReadPos()) + ")");
#endif
                        bridge->write(wrappedBuffer);
                    }

                    bridge->writeAllInQueue();
                } else {
                    // Close itself
#ifdef NDEBUG
#else
                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__,"Other main already set. close cbId: "  + to_string(newCbId));
#endif
                    _removeBridge(bridge->vpnCbId(), bridge->connectAddr().networkType());
                    bridge->close(newCbId, true);
                }
            }
        });
    }
}

void
L4::AnyfiBridgeApplication::_onReadFromVPNSession(unsigned short cbId, const NetworkBufferPointer &buffer) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList == nullptr) {
        _L4->closeSession(cbId, true);
    } else {
        for (auto bridge : *bridgeList) {
            if (bridge->type() == AnyfiBridge::Type::VpnToProxy) {
                NetworkBufferPointer bufferCopy = NetworkBufferPool::acquire();
                bufferCopy->copyFrom(buffer.get());

#ifdef NDEBUG
#else
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "write vpn to proxy " + to_string(bridge->vpnCbId()) + " -> " + to_string(bridge->proxyCbId()) + " (buffer size: " + to_string(bufferCopy->getWritePos() - bufferCopy->getReadPos()) + ") ");
#endif

                bridge->write(bufferCopy);

            } else if (bridge->type() == AnyfiBridge::Type::VpnToMesh) {
                auto payloadBuffer = NetworkBufferPool::acquire();
                payloadBuffer->setBackwardMode();
                auto buf = buffer->isBackwardMode() ? buffer->buffer() : buffer->buffer() + buffer->getReadPos();
                payloadBuffer->putBytes(buf, buffer->getWritePos() - buffer->getReadPos());

                std::shared_ptr<AnyfiBridgeDataPacket> dataPacket = std::make_shared<AnyfiBridgeDataPacket>(payloadBuffer);
                NetworkBufferPointer wrappedBuffer = dataPacket->toPacket();

#ifdef NDEBUG
#else
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "write vpn to mesh " + to_string(bridge->vpnCbId()) + " -> " + to_string(bridge->meshCbId()) + " (buffer size: " + to_string(wrappedBuffer->getWritePos() - wrappedBuffer->getReadPos()) + ")");
#endif

                bridge->write(wrappedBuffer);
            }
        }
    }
}

void L4::AnyfiBridgeApplication::onSessionConnectFromMesh(unsigned short meshCbId, std::vector<Anyfi::Address> gatewayList, bool useMain) {
    if (_bridgeMap.get(meshCbId) != nullptr) {
        throw std::runtime_error("duplicated control block: " + to_string(meshCbId));
    } else {
        // Construct bridge
        auto bridgeList = std::make_shared<std::vector<std::shared_ptr<AnyfiBridge>>>();
        for (auto gwAddr : gatewayList) {
            auto newBridge = std::make_shared<AnyfiBridge>(_L4);
            newBridge->meshCbId(meshCbId);

            if (gwAddr.networkType() == Anyfi::NetworkType::WIFI || gwAddr.networkType() == Anyfi::NetworkType::MOBILE || gwAddr.networkType() == Anyfi::NetworkType::DEFAULT) {
                newBridge->type(AnyfiBridge::Type::MeshToProxy);
                // Save network type until Bridge Connect Packet received
                newBridge->connectAddr(gwAddr);
            }

            bridgeList->insert(bridgeList->end(), newBridge);
        }
        if (useMain) {
            bridgeList->front()->setMain(true);
        }
        _bridgeMap.addBridge(meshCbId, bridgeList);
    }
}

void L4::AnyfiBridgeApplication::_onReadFromMeshSession(unsigned short cbId, NetworkBufferPointer buffer) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList == nullptr) {
        _L4->closeSession(cbId, true);
    } else {
        for (auto bridge : *bridgeList) {
            NetworkBufferPointer bufferCopy = NetworkBufferPool::acquire();
            bufferCopy->copyFrom(buffer.get());

            // Check Bridge Packet Type
            auto type = AnyfiBridgePacket::getType(bufferCopy);
            if (type == AnyfiBridgePacket::Type::Connect) {
                if (bridge->type() == AnyfiBridge::Type::VpnToMesh) {
                    throw std::runtime_error("Unsupported Mesh Connect: " + to_string(cbId));
                } else if (bridge->type() == AnyfiBridge::Type::MeshToProxy) {
                    _processMeshConnect(bridge, bufferCopy);
                }
            } else if (type == AnyfiBridgePacket::Type::Data) {
                if (bridge->type() == AnyfiBridge::Type::VpnToMesh) {
                    if (bridge->isMain()) {
                        _processMeshRead(bridge, bufferCopy);
                    }
                } else if (bridge->type() == AnyfiBridge::Type::MeshToProxy) {
                    _processMeshRead(bridge, bufferCopy);
                }
            } else {
                Anyfi::Log::error(__func__, "Invalid packet type");
            }
        }
    }
}

void L4::AnyfiBridgeApplication::onSessionConnectFromProxy(unsigned short cbId, Anyfi::Address sourceAddr) {
    throw std::runtime_error("Unsupported Proxy Connect.");
}

void L4::AnyfiBridgeApplication::_onReadFromProxySession(unsigned short cbId, NetworkBufferPointer buffer) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList == nullptr) {
        _L4->closeSession(cbId, true);
    } else {
        for (auto bridge : *bridgeList) {
            if (bridge->type() == AnyfiBridge::Type::VpnToProxy) {
                if (bridge->isMain()) {
#ifdef NDEBUG
#else
                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "write proxy to vpn " + to_string(bridge->proxyCbId()) + " -> " + to_string(bridge->vpnCbId()) + " (buffer size: " + to_string(buffer->getWritePos() - buffer->getReadPos()) + ")");
#endif
                    // Set backward mode
                    auto newBuffer = NetworkBufferPool::acquire();
                    newBuffer->setBackwardMode();
                    newBuffer->putBytes(buffer.get());
                    bridge->read(newBuffer);
                }
            } else if (bridge->type() == AnyfiBridge::Type::MeshToProxy) {
                if (bridge->isMain()) {
                    auto dataPacket = std::make_shared<AnyfiBridgeDataPacket>(buffer);
                    NetworkBufferPointer wrappedBuffer = dataPacket->toPacket();

#ifdef NDEBUG
#else
                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "write proxy to mesh " + to_string(bridge->proxyCbId()) + " -> " + to_string(bridge->meshCbId()) + " (buffer size: " + to_string(wrappedBuffer->getWritePos() - wrappedBuffer->getReadPos()) + ")");
#endif

                    bridge->read(wrappedBuffer);
                }
            }
        }
    }
}

void L4::AnyfiBridgeApplication::_processMeshConnect(std::shared_ptr<AnyfiBridge> bridge, const NetworkBufferPointer &buffer) {
    // May assume connecting to Proxy
    auto connPacket = AnyfiBridgeConnPacket::toSerialize(buffer);
    auto destAddr = connPacket->addr();
    destAddr.connectionType(Anyfi::ConnectionType::Proxy);

    // Set network type
    destAddr.networkType(bridge->connectAddr().networkType());
    bridge->connectAddr(destAddr);

    if (!_addressOptionValidCheck(Anyfi::ConnectionType::Proxy, destAddr)) {
        throw std::invalid_argument("Invalid proxy destination address option");
    }

#ifdef NDEBUG
#else
    if (bridge->connectAddr().networkType() == Anyfi::NetworkType::WIFI) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect(Wi-Fi) to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()));
    } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::P2P) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect(P2P) to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()));
    } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::MOBILE) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect(Mobile) to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()));
    } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::DEFAULT) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, to_string(bridge->vpnCbId()) + " try connect to " + bridge->connectAddr().addr() + ":" + to_string(bridge->connectAddr().port()));
    }
#endif

    // Connect bridge
    bridge->connect(bridge->connectAddr(), [bridge, this](unsigned short newCbId) {
        if (newCbId == 0) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connect failed for Mesh: " + to_string(bridge->meshCbId()));
#endif
            _handleSessionClosedByDest(bridge, false);
        } else if (bridge->meshCbId() == 0) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected, but Mesh already closed for cbId: " + to_string(newCbId));
#endif
            bridge->closeAll(true);
        } else {
            // Connected. Add other side's bridge
            if (bridge->type() == AnyfiBridge::Type::MeshToProxy) {
#ifdef NDEBUG
#else
                if (bridge->connectAddr().networkType() == Anyfi::NetworkType::WIFI) {
                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected proxy (Wi-Fi): " + to_string(bridge->meshCbId()) + " -> " + to_string(bridge->proxyCbId()));
                } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::P2P) {
                    throw std::invalid_argument("Unsupported P2P to P2P bridge connection");
                } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::MOBILE) {
                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected proxy (Mobile): " + to_string(bridge->meshCbId()) + " -> " + to_string(bridge->proxyCbId()));
                } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::DEFAULT) {
                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "connected proxy: " + to_string(bridge->meshCbId()) + " -> " + to_string(bridge->proxyCbId()));
                }
#endif

                bridge->proxyCbId(newCbId);

                auto singleBridgeList = std::make_shared<std::vector<std::shared_ptr<AnyfiBridge>>>();
                singleBridgeList->insert(singleBridgeList->end(), bridge);
                _bridgeMap.addBridge(newCbId, singleBridgeList);
            }
        }
    });
}
void L4::AnyfiBridgeApplication::_processMeshRead(std::shared_ptr<AnyfiBridge> bridge,
                                                  const NetworkBufferPointer &buffer) {
    auto dataPacket = AnyfiBridgeDataPacket::toSerialize(buffer);
    auto payloadBuffer = NetworkBufferPool::acquire();
    payloadBuffer->setBackwardMode();
    auto buf = buffer->isBackwardMode() ? buffer->buffer() : buffer->buffer() + buffer->getReadPos();
    payloadBuffer->putBytes(buf, buffer->getWritePos() - buffer->getReadPos());

    if (bridge->type() == AnyfiBridge::Type::MeshToProxy) {
        // write to proxy
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "write mesh to proxy " + to_string(bridge->meshCbId()) + " -> " + to_string(bridge->proxyCbId()) + " (buffer size: " + to_string(buffer->getWritePos() - buffer->getReadPos()) + ")");
#endif
        bridge->write(payloadBuffer);
    } else if (bridge->type() == AnyfiBridge::Type::VpnToMesh) {
        // write to vpn
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "write mesh to vpn " + to_string(bridge->meshCbId()) + " -> " + to_string(bridge->vpnCbId()) + " (buffer size: " + to_string(payloadBuffer->getWritePos() - payloadBuffer->getReadPos()) + ")");
#endif
        bridge->read(payloadBuffer);
    }
}
bool L4::AnyfiBridgeApplication::_addressOptionValidCheck(Anyfi::ConnectionType type, const Anyfi::Address &addr) {
    if (type == Anyfi::ConnectionType::Proxy || type == Anyfi::ConnectionType::VPN) {
        if (addr.connectionType() != Anyfi::ConnectionType::Proxy && addr.connectionType() != Anyfi::ConnectionType::VPN) {
            Anyfi::Log::error(__func__, "Wrong connection type: " + Anyfi::Address::toString(addr.connectionType()) + ", expect: " + Anyfi::Address::toString(type));
            return false;
        }
        if (addr.type() != Anyfi::AddrType::IPv4 && addr.type() != Anyfi::AddrType::IPv6) {
            Anyfi::Log::error(__func__, "wrong address type: " + Anyfi::Address::toString(addr.type()) + ", expect: IPv4 or IPv6");
            return false;
        }
        if (addr.connectionProtocol() != Anyfi::ConnectionProtocol::TCP && addr.connectionProtocol() != Anyfi::ConnectionProtocol::UDP) {
            Anyfi::Log::error(__func__, "wrong address connection protocol: " + Anyfi::Address::toString(addr.connectionProtocol()) + ", expect: TCP or UDP");
            return false;
        }
    }else if (type == Anyfi::ConnectionType::BluetoothP2P) {
        if (addr.connectionType() != Anyfi::ConnectionType::BluetoothP2P) {
            Anyfi::Log::error(__func__, "Wrong connection type: " + Anyfi::Address::toString(addr.connectionType()) + ", expect: " + Anyfi::Address::toString(type));
            return false;
        }
        if (addr.type() != Anyfi::AddrType::BLUETOOTH_UID) {
            Anyfi::Log::error(__func__, "wrong address type: " + Anyfi::Address::toString(addr.type()) + ", expect: BLUETOOTH_UID");
            return false;
        }
        if (addr.connectionProtocol() != Anyfi::ConnectionProtocol::MeshRUDP && addr.connectionProtocol() != Anyfi::ConnectionProtocol::MeshUDP) {
            Anyfi::Log::error(__func__, "wrong address connection protocol: " + Anyfi::Address::toString(addr.connectionProtocol()) + ", expect: MeshRUDP or MeshUDP");
            return false;
        }
    }else if (type == Anyfi::ConnectionType::WifiP2P) {
        if (addr.connectionType() != Anyfi::ConnectionType::WifiP2P) {
            Anyfi::Log::error(__func__, "Wrong connection type: " + Anyfi::Address::toString(addr.connectionType()) + ", expect: " + Anyfi::Address::toString(type));
            return false;
        }
        if (addr.type() != Anyfi::AddrType::MESH_UID) {
            Anyfi::Log::error(__func__, "wrong address type: " + Anyfi::Address::toString(addr.type()) + ", expect: MESH_UID");
            return false;
        }
        if (addr.connectionProtocol() != Anyfi::ConnectionProtocol::MeshRUDP && addr.connectionProtocol() != Anyfi::ConnectionProtocol::MeshUDP) {
            Anyfi::Log::error(__func__, "wrong address connection protocol: " + Anyfi::Address::toString(addr.connectionProtocol()) + ", expect: MeshRUDP or MeshUDP");
            return false;
        }
    }else {
        Anyfi::Log::error(__func__, "Unknown type.");
        return false;
    }
    return true;
}
void L4::AnyfiBridgeApplication::onSessionClose(unsigned short cbId, bool forced) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList != nullptr && bridgeList->size() > 0) {
        auto firstBridge = bridgeList->front();
        if (firstBridge->vpnCbId() == cbId) {
            _onSessionCloseFromVPN(cbId, forced);
        } else if (firstBridge->meshCbId() == cbId) {
            _onSessionCloseFromMesh(cbId, forced);
        } else if (firstBridge->proxyCbId() == cbId) {
            _onSessionCloseFromProxy(cbId, forced);
        } else {
            throw std::runtime_error("onSessionClose - cbId does not belong :" + to_string(cbId));
        }
    }
}


void L4::AnyfiBridgeApplication::_onSessionCloseFromVPN(unsigned short cbId, bool forced) {
#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "cbId: " + to_string(cbId));
#endif

    // Remove all bridges and close all
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList != nullptr) {
        for (auto bridge : *bridgeList) {
            bridge->vpnCbId(0);
            if (bridge->type() == AnyfiBridge::Type::VpnToMesh && bridge->meshCbId() != 0) {
                _bridgeMap.removeBridge(bridge->meshCbId());
            } else if (bridge->type() == AnyfiBridge::Type::VpnToProxy && bridge->proxyCbId() != 0) {
                _bridgeMap.removeBridge(bridge->proxyCbId());
            }

            bridge->closeAll(forced);
        }
        _bridgeMap.removeBridge(cbId);
    }
}

void L4::AnyfiBridgeApplication::_onSessionCloseFromProxy(unsigned short cbId, bool forced) {
#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "cbId: " + to_string(cbId));
#endif

    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList != nullptr) {
        for (auto bridge : *bridgeList) {
            if (bridge->type() == AnyfiBridge::Type::VpnToProxy && bridge->vpnCbId() != 0) {
                _handleSessionClosedByDest(bridge, forced);
            } else if (bridge->type() == AnyfiBridge::Type::MeshToProxy && bridge->meshCbId() != 0) {
                _handleSessionClosedByDest(bridge, forced);
            }
        }
        _bridgeMap.removeBridge(cbId);
//        _bridgeMap.erase(cbId);
    }
}

void L4::AnyfiBridgeApplication::_onSessionCloseFromMesh(unsigned short cbId, bool forced) {
#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "cbId: " + to_string(cbId));
#endif

    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList != nullptr) {
        for (auto bridge : *bridgeList) {
            if (bridge->type() == AnyfiBridge::Type::VpnToMesh && bridge->vpnCbId() != 0) {
                _handleSessionClosedByDest(bridge, forced);
            } else if (bridge->type() == AnyfiBridge::Type::MeshToProxy && bridge->proxyCbId() != 0) {
                _bridgeMap.removeBridge(bridge->proxyCbId());
                bridge->closeAll(forced);
            }
        }
        _bridgeMap.removeBridge(cbId);
    }
}

void L4::AnyfiBridgeApplication::onReadFromSession(unsigned short cbId, const NetworkBufferPointer &buffer) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList == nullptr) {
        _L4->closeSession(cbId, true);
    } else {
        auto firstBridge = bridgeList->front();
        if (firstBridge->vpnCbId() == cbId) {
            _onReadFromVPNSession(cbId, buffer);
        } else if (firstBridge->meshCbId() == cbId) {
            _onReadFromMeshSession(cbId, buffer);
        } else if (firstBridge->proxyCbId() == cbId) {
            _onReadFromProxySession(cbId, buffer);
        } else {
            throw std::runtime_error("onReadFromSession - cbId does not belong :" + to_string(cbId));
        }
    }
}

void L4::AnyfiBridgeApplication::_handleSessionClosedByDest(std::shared_ptr<AnyfiBridge> bridge, bool forced) {
    // Handle related sessions on a session closed
    auto sourceCbId = bridge->getSourceCbId();
    auto destCbId = bridge->getDestCbId();
    if (bridge->isMain()) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__,"Main closed "  + to_string(destCbId) + " -> " + to_string(sourceCbId));
#endif
        if (destCbId == 0) {
            // Do not modify bridge list for graceful shutdown on connect fail
            _closeNonMainBridges(sourceCbId, false);
        } else {
            _closeNonMainBridges(sourceCbId, true);
        }
        _bridgeMap.removeBridge(sourceCbId);
        bridge->closeAll(forced);
    } else {
        int bridgesLeft = _removeBridge(sourceCbId, bridge->connectAddr().networkType());
        if (bridgesLeft == 0) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__,"No bridges left for cbId: "  + to_string(sourceCbId));
#endif
            _bridgeMap.removeBridge(sourceCbId);
            bridge->closeAll(forced);
        } else {;
            // Close itself only
            bridge->close(destCbId, false);
        }
    }
}

void L4::AnyfiBridgeApplication::_closeNonMainBridges(unsigned short cbId, bool eraseFromList) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList != nullptr) {
        for (auto it = bridgeList->begin(); it != bridgeList->end();) {
            if (!(*it)->isMain()) {
                auto removeCbId = 0;
                if ((*it)->type() == AnyfiBridge::Type::VpnToMesh) {
                    removeCbId = (*it)->meshCbId();
                    (*it)->vpnCbId(0);
                } else if ((*it)->type() == AnyfiBridge::Type::VpnToProxy) {
                    removeCbId = (*it)->proxyCbId();
                    (*it)->vpnCbId(0);
                } else if ((*it)->type() == AnyfiBridge::Type::MeshToProxy) {
                    removeCbId = (*it)->proxyCbId();
                    (*it)->meshCbId(0);
                }

                _bridgeMap.removeBridge(removeCbId);
                (*it)->closeAll(false);

                if (eraseFromList) {
#ifdef NDEBUG
#else
                    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__,"remove bridge: " + to_string(cbId) + " -> " + to_string(removeCbId));
#endif
                    it = bridgeList->erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
}

bool L4::AnyfiBridgeApplication::_hasMain(unsigned short cbId) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList != nullptr) {
        for (auto bridge : *bridgeList) {
            if (bridge->isMain()) {
                return true;
            }
        }
    }
    return false;
}

int L4::AnyfiBridgeApplication::_removeBridge(unsigned short cbId, Anyfi::NetworkType networkType) {
    auto bridgeList = _bridgeMap.get(cbId);
    if (bridgeList != nullptr) {
        for (auto it = bridgeList->begin(); it != bridgeList->end();) {
            if ((*it)->connectAddr().networkType() == networkType) {
#ifdef NDEBUG
#else
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__,"remove bridge: " + to_string(cbId) + " -> " + to_string((*it)->getDestCbId()));
#endif
                it = bridgeList->erase(it);
                break;
            } else {
                ++it;
            }
        }
        return bridgeList->size();
    }
    return 0;
}

void L4::AnyfiBridgeApplication::resetSession(std::unordered_map<int, int> srcPortUidMap) {
    std::vector<unsigned short> cbIdList = _bridgeMap.getKeys();

    // Get destination cbId of sessions to reset
    std::set<unsigned short> resetCbIdSet;
    for (unsigned short cbId : cbIdList) {
        auto bridgeList = _bridgeMap.get(cbId);
        for (auto bridge : *bridgeList) {
            // Find app uid
            int uid = -1;
            auto uidEntry = srcPortUidMap.find(bridge->srcAddr().port());
            if (uidEntry != srcPortUidMap.end()) {
                uid = uidEntry->second;
            } else {
                uid = _L4->getUidForAddr(bridge->srcAddr(), bridge->connectAddr());
            }

            // Find new network traffic
            Anyfi::NetworkType newTrafficType = _L4->getTrafficTypeForUid(uid);

            bool reset = false;
            if (bridge->connectAddr().networkType() == Anyfi::NetworkType::WIFI) {
                if (newTrafficType != Anyfi::NetworkType::WIFI && newTrafficType != Anyfi::NetworkType::DUAL) {
                    reset = true;
                }
            } else if (bridge->connectAddr().networkType() == Anyfi::NetworkType::MOBILE) {
                if (newTrafficType != Anyfi::NetworkType::MOBILE && newTrafficType != Anyfi::NetworkType::MOBILE_WITH_WIFI_TEST && newTrafficType != Anyfi::NetworkType::DUAL) {
                    reset = true;
                }
            } else {
                reset = true;
            }

            if (reset) {
                resetCbIdSet.insert(bridge->getSourceCbId());
#ifdef NDEBUG
#else
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L4, __func__, "reset(" +
                         std::string(bridge->connectAddr().connectionProtocol() == Anyfi::ConnectionProtocol::UDP ? "UDP" : bridge->connectAddr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP ? "TCP" :
                          bridge->connectAddr().connectionProtocol() == Anyfi::ConnectionProtocol::MeshUDP ? "MeshUDP" : bridge->connectAddr().connectionProtocol() == Anyfi::ConnectionProtocol::MeshRUDP ? "MeshRUDP" :"N/A") + " - " +
                        std::string(bridge->connectAddr().networkType() == Anyfi::NetworkType::WIFI ? "WIFI" : bridge->connectAddr().networkType() == Anyfi::NetworkType::MOBILE ? "Mobile" : bridge->connectAddr().networkType() == Anyfi::NetworkType::P2P ? "P2P" : bridge->connectAddr().networkType() == Anyfi::NetworkType::DEFAULT ? "DEFAULT" : "N/A") +
                        " => " + std::string(newTrafficType == Anyfi::NetworkType::WIFI ? "WIFI" : newTrafficType == Anyfi::NetworkType::MOBILE ? "Mobile" : newTrafficType == Anyfi::NetworkType::MOBILE_WITH_WIFI_TEST ? "Mobile+Test" : newTrafficType == Anyfi::NetworkType::DUAL ? "Dual" : newTrafficType == Anyfi::NetworkType::NONE ? "None" : "N/A") +
                         ") cbId: " + to_string(cbId) + " uid: " + to_string(uid));
#endif
            }
        }
    }

    for (auto cbId: resetCbIdSet) {
        _L4->closeSession(cbId, true);
        onSessionClose(cbId, true);
    }
}

void L4::AnyfiBridgeApplication::dump() {
    std::vector<unsigned short> cbIdList = _bridgeMap.getKeys();

    std::unordered_map<AnyfiBridge::Type, std::unordered_map<Anyfi::ConnectionProtocol, std::unordered_map<Anyfi::NetworkType, int>>> countMap;
    for (unsigned short cbId : cbIdList) {
        auto bridgeList = _bridgeMap.get(cbId);
        for (auto bridge : *bridgeList) {
            auto bridgeTypeEntry = countMap.find(bridge->type());
            std::unordered_map<Anyfi::ConnectionProtocol, std::unordered_map<Anyfi::NetworkType, int>> connProtoMap;
            if (bridgeTypeEntry != countMap.end()){
                connProtoMap = bridgeTypeEntry->second;
            }
            auto connProtoEntry = connProtoMap.find(bridge->connectAddr().connectionProtocol());
            std::unordered_map<Anyfi::NetworkType, int> netTypeMap;
            if (connProtoEntry != connProtoMap.end()) {
                netTypeMap = connProtoEntry->second;
            }
            auto netTypeEntry = netTypeMap.find(bridge->connectAddr().networkType());
            int count = 0;
            if (netTypeEntry != netTypeMap.end()) {
                count = netTypeEntry->second;
            }
            netTypeMap[bridge->connectAddr().networkType()] = count + 1;
            connProtoMap[bridge->connectAddr().connectionProtocol()] = netTypeMap;
            countMap[bridge->type()] = connProtoMap;
        }
    }

    for (auto &typeEntry : countMap) {
        std::string result = "";
        switch (typeEntry.first) {
            case AnyfiBridge::Type::VpnToMesh:
                result += "VpnToMesh - ";
                break;
            case AnyfiBridge::Type::VpnToProxy:
                result += "VpnToProxy - ";
                break;
            case AnyfiBridge::Type::MeshToProxy:
                result += "MeshToProxy - ";
                break;
            default:
                break;
        }
        for (auto &protocolEntry : typeEntry.second) {
            for (auto &netTypeEntry: protocolEntry.second) {
                switch (protocolEntry.first) {
                    case Anyfi::ConnectionProtocol::UDP:
                        result += "UDP(";
                        break;
                    case Anyfi::ConnectionProtocol::TCP:
                        result += "TCP(";
                        break;
                    case Anyfi::ConnectionProtocol::MeshUDP:
                        result += "MeshUDP(";
                        break;
                    case Anyfi::ConnectionProtocol::MeshRUDP:
                        result += "MeshRUDP(";
                        break;
                }
                switch (netTypeEntry.first) {
                    case Anyfi::NetworkType::WIFI:
                        result += "WIFI): " + to_string(netTypeEntry.second) + ", ";
                        break;
                    case Anyfi::NetworkType::MOBILE:
                        result += "Mobile): " + to_string(netTypeEntry.second) + ", ";
                        break;
                    case Anyfi::NetworkType::P2P:
                        result += "P2P): " + to_string(netTypeEntry.second) + ", ";
                        break;
                    case Anyfi::NetworkType::DEFAULT:
                        result += "DEFAULT): " + to_string(netTypeEntry.second) + ", ";
                        break;
                    default:
                        break;
                }
            }
        }
        Anyfi::Log::error(__func__, result);
    }
}
