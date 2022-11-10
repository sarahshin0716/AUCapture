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
#include <L3TransportLayer/Packet/TCPIP_L2_Internet/IPv6Header.h>
#include "L3TransportLayer.h"

#include "L3TransportLayer/Packet/TCPIP_L2_Internet/IPv4Header.h"
#include "L3TransportLayer/Packet/TCPIP_L4_Application/MeshProtocolHeader.h"


L3TransportLayer::L3TransportLayer(Anyfi::UUID myUid) : _myUid(myUid) {
}

L3TransportLayer::~L3TransportLayer() {

}

void L3TransportLayer::initialize() {
    _VPNTCP = std::make_shared<L3::VPNTCPProcessor>(shared_from_this());
    _VPNUDP = std::make_shared<L3::VPNUDPProcessor>(shared_from_this());
    _ProxyTCP = std::make_shared<L3::ProxyTCPProcessor>(shared_from_this());
    _ProxyUDP = std::make_shared<L3::ProxyUDPProcessor>(shared_from_this());
    _MeshRUDP = L3::MeshEnetRUDPProcessor::init(shared_from_this(), _myUid);
    _MeshUDP = std::make_shared<L3::MeshUDPProcessor>(shared_from_this(), _myUid);
}

void L3TransportLayer::terminate() {
    _VPNTCP->shutdown();
    _VPNUDP->shutdown();
    _ProxyTCP->shutdown();
    _ProxyUDP->shutdown();
    _MeshRUDP->shutdown();
    _MeshUDP->shutdown();
}

void L3TransportLayer::reset() {
    // TODO:
    _MeshRUDP->reset();
}

//
// ========================================================================================
// Override interfaces : IL3TransportLayerForL2
//
void
L3TransportLayer::onReadFromMesh(Anyfi::Address srcAddr, NetworkBufferPointer buffer) {
    if (!srcAddr.isP2PConnectionType())
        throw std::invalid_argument("Address should be P2P connection type");

    if (buffer->size() == 0)
        Anyfi::Log::error(__func__, "L3:: buffer size == 0???");

    auto meshPacket = std::make_shared<MeshProtocolHeader>(buffer, buffer->getReadPos());
    auto meshProtocol = meshPacket->protocol();
    if (meshProtocol == MeshProtocol::RUDP) {
        _MeshRUDP->read(buffer, srcAddr);
    } else if (meshProtocol == MeshProtocol::UDP) {
        _MeshUDP->read(buffer, srcAddr);
    } else if (meshProtocol == MeshProtocol::ICMP) {

    } else {
        Anyfi::Log::error("L3", std::string(__func__) + ":: Invalid Protocol");
    }
}

void
L3TransportLayer::onReadFromProxy(Anyfi::Address address, NetworkBufferPointer buffer,
                                  unsigned short channelId) {
    if (address.connectionType() != Anyfi::ConnectionType::Proxy)
        throw std::invalid_argument("Address should be Proxy connection type");
    if (address.isMeshProtocol())
        throw std::invalid_argument("Address could not be Mesh connection protocol.");

    if (buffer->size() == 0)
        Anyfi::Log::error(__func__, "L3:: buffer size == 0???");

    if (address.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
        _ProxyTCP->read(buffer, address, channelId);
    } else if (address.connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
        _ProxyUDP->read(buffer, address, channelId);
    }
}

void L3TransportLayer::onReadFromVPN(NetworkBufferPointer buffer) {
    auto ipPacket = std::make_shared<IPPacket>(buffer, buffer->getReadPos());
    auto *ipv4Header = dynamic_cast<IPv4Header *>(ipPacket->header().get());
    if (ipv4Header && ipv4Header->version() == IPVersion::Version4) {
        switch (ipv4Header->protocol()) {
            case IPNextProtocol::TCP:
                _VPNTCP->input(ipPacket);
                break;
            case IPNextProtocol::UDP:
                _VPNUDP->input(ipPacket);
                break;
            default:
                // Not yet supported
                Anyfi::Log::warn(__func__, "IPv4 Unsupported Transport Layer Protocol: " + to_string(unsigned(ipv4Header->protocol())));
                break;
        }
    } else {
        auto *ipv6Header = dynamic_cast<IPv6Header *>(ipPacket->header().get());
        if (ipv6Header && ipv6Header->version() == IPVersion::Version6) {
            switch (ipv6Header->nextHeader()) {
                case IPNextProtocol::TCP:
                    _VPNTCP->input(ipPacket);
                    break;
                case IPNextProtocol::UDP:
                    _VPNUDP->input(ipPacket);
                    break;
                default:
                    // Not yet supported
                    Anyfi::Log::warn(__func__, "IPv6 Unsupported Transport Layer Protocol: " + to_string(unsigned(ipv6Header->nextHeader())));
                    break;
            }
        } else {
            Anyfi::Log::warn(__func__, "Unsupported IP version: " + to_string(ipv6Header == nullptr ? -1 : (uint8_t) ipv6Header->version()));
        }
    }
}

void L3TransportLayer::onChannelClosed(unsigned short channelId, Anyfi::Address address) {
    if (address.connectionType() == Anyfi::ConnectionType::Proxy) {
        auto key = ControlBlockKey(L3::ProxyPacketProcessor::srcAddr(channelId), address);
        auto cb = _findControlBlock(key);
        if (cb == nullptr) {
            Anyfi::Log::error(__func__, "cb is nullptr: " + to_string(channelId));
            return;
        }

        if (instanceof<ProxyTCB>(cb.get())) {
            _ProxyTCP->close(key, true);
        } else if (instanceof<ProxyUCB>(cb.get())) {
            _ProxyUDP->close(key, true);
        } else {
            throw std::runtime_error("It should be proxy control block.");
        }

        // Notify proxy session is closed
        _L4->onSessionClose(cb->id(), false);
    } else if (address.isP2PConnectionType()) {
        // TODO : Neighbor mesh node도 onMeshNodeClosed에 포함되서 올라오도록 수정
        // 아래 onMeshNodeClosed 추가 호출 필요 없도록 코드 수정 필요
        auto set = std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher>();
        set.insert(address);
        onMeshNodeClosed(set);

        _L4->onMeshLinkClosed(address);
    }
}

void L3TransportLayer::onMeshNodeClosed(std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses) {
    for (const auto &destAddr: addresses) {
        auto cbs = _findControlBlocksWithDestAddr(destAddr);
        if (cbs.empty())
            continue;

        for (const auto &cb : cbs) {
            if (instanceof<L3::MeshEnetRUCB>(cb.get())) {
                _MeshRUDP->close(cb->key(), true);
            } else if (instanceof<MeshUCB>(cb.get())) {
                _MeshUDP->close(cb->key(), true);
            } else {
                throw std::runtime_error("It should be mesh control block");
            }

            // Notify mesh session is closed
            _L4->onSessionClose(cb->id(), true);
        }
    }
}
//
// Override interfaces : IL3TransportLayerForL2
// ========================================================================================
//


//
// ========================================================================================
// Override interfaces : IL3TransportLayerForL4
//
void L3TransportLayer::openServerLink(Anyfi::Address addr,
                                      const std::function<void(Anyfi::Address address)> &callback) {
    auto assignedAddr = _L2->open(addr);
    callback(assignedAddr);
}

void L3TransportLayer::closeServerLink(Anyfi::Address addr) {

}

void L3TransportLayer::connectMeshLink(Anyfi::Address address,
                                       const std::function<void(Anyfi::Address address)> &callback) {
    _L2->connectMesh(address, callback);
}

void L3TransportLayer::closeMeshLink(Anyfi::Address address) {
    // TODO: 추후에 L2 close return 으로 연결 해제된 모든 노드(Neighbor 포함)를 돌려주고, 별도의 onClose callback은 올리지 않게 수정
    _L2->close(address);

    auto set = std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher>();
    set.insert(address);
    onMeshNodeClosed(set);

}

void L3TransportLayer::connectSession(Anyfi::Address address,
                                      std::function<void(unsigned short)> callback) {
    if (address.isP2PConnectionType()) {
        if (!address.isMeshProtocol())
            throw std::invalid_argument("Address should be mesh protocol");

        if (address.connectionProtocol() == Anyfi::ConnectionProtocol::MeshUDP) {
            _MeshUDP->connect(address, [callback](std::shared_ptr<ControlBlock> cb) {
                callback(cb != nullptr ? cb->id() : 0);
            });
        } else if (address.connectionProtocol() == Anyfi::ConnectionProtocol::MeshRUDP) {
            _MeshRUDP->connect(address, [callback](std::shared_ptr<ControlBlock> cb) {
                callback(cb != nullptr ? cb->id() : 0);
            });
        } else
            throw std::invalid_argument("Unsupported connection protocol in P2P connection type");
    } else if (address.connectionType() == Anyfi::ConnectionType::Proxy) {
        if (address.isMeshProtocol())
            throw std::invalid_argument("Address could not be mesh protocol");

        if (address.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            _ProxyTCP->connect(address, [callback](std::shared_ptr<ControlBlock> cb) {
                callback(cb != nullptr ? cb->id() : 0);
            });
        } else if (address.connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
            _ProxyUDP->connect(address, [callback](std::shared_ptr<ControlBlock> cb) {
                callback(cb != nullptr ? cb->id() : 0);
            });
        } else
            throw std::invalid_argument("Unsupported connection type in Proxy connection type.");
    } else {
        throw std::invalid_argument("Unsupported connection type");
    }
}

void L3TransportLayer::closeSession(unsigned short cb_id, bool force) {
    std::shared_ptr<ControlBlock> cb = _findControlBlock(cb_id);

    if (cb != nullptr) {
        if (instanceof<VpnTCB>(cb.get())) {
            _VPNTCP->close(cb->key(), force);
        } else if (instanceof<VpnUCB>(cb.get())) {
            _VPNUDP->close(cb->key(), force);
        } else if (instanceof<ProxyTCB>(cb.get())) {
            _ProxyTCP->close(cb->key(), force);

            // Proxy channel id is the source port in control block key.
            _L2->close(cb->key().srcAddress().port());
        } else if (instanceof<ProxyUCB>(cb.get())) {
            _ProxyUDP->close(cb->key(), force);

            // Proxy channel id is the source port in control block key.
            _L2->close(cb->key().srcAddress().port());
        } else if (instanceof<L3::MeshEnetRUCB>(cb.get())) {
            _MeshRUDP->close(cb->key(), force);
        } else if (instanceof<MeshUCB>(cb.get())) {
            _MeshUDP->close(cb->key(), force);
        }
    }
}

void L3TransportLayer::write(unsigned short cbId, NetworkBufferPointer buffer) {
    std::shared_ptr<ControlBlock> cb = _findControlBlock(cbId);
    if (cb == nullptr) {
        Anyfi::Log::error(__func__, "cb(id:" + to_string(cbId) + ") is nullptr");
        return;
    }

    if (instanceof<VpnTCB>(cb.get()))
        _VPNTCP->write(buffer, cb->key());
    else if (instanceof<VpnUCB>(cb.get()))
        _VPNUDP->write(buffer, cb->key());
    else if (instanceof<ProxyTCB>(cb.get()))
        _ProxyTCP->write(buffer, cb->key());
    else if (instanceof<ProxyUCB>(cb.get()))
        _ProxyUDP->write(buffer, cb->key());
    else if (instanceof<MeshUCB>(cb.get()))
        _MeshUDP->write(buffer, cb->key());
    else if (instanceof<L3::MeshEnetRUCB>(cb.get()))
        _MeshRUDP->write(buffer, cb->key());
    else
        Anyfi::Log::error(__func__, "Unknown CBid");
}

void L3TransportLayer::setMeshNodeType(uint8_t type) {
    _L2->setMeshNodeType(type);
}

std::pair<Anyfi::Address, uint8_t> L3TransportLayer::getClosestMeshNode(uint8_t type) {
    return _L2->getClosestMeshNode(type);
}

void L3TransportLayer::openExternalVPN() {
    _L2->openVPN();
}

void L3TransportLayer::openInternalVPN(int fd) {
    _L2->openVPN(fd);
}

void L3TransportLayer::closeVPN() {
    _L2->stopVPN();
    _VPNTCP->clear();
    _VPNUDP->clear();
}
//
// Override interfaces : IL3TransportLayerForL4
// ========================================================================================
//


//
// ========================================================================================
// Override interfaces : L3::IL3ForPacketProcessor
//
std::shared_ptr<ControlBlock> L3TransportLayer::getControlBlock(unsigned short id) {
    return _findControlBlock(id);
}

std::shared_ptr<ControlBlock> L3TransportLayer::getControlBlock(ControlBlockKey &key) {
    return _findControlBlock(key);
}

void L3TransportLayer::onControlBlockCreated(std::shared_ptr<ControlBlock> cb) {
    cb->assignID(_getUnassignedCbId());
    _addControlBlock(cb, cb->key());
}

void L3TransportLayer::onControlBlockDestroyed(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_cbMapMutex);

    _removeControlBlockWithoutLock(key);
}

void L3TransportLayer::onSessionConnected(std::shared_ptr<ControlBlock> cb) {
    if (_L4 == nullptr)
        return;

    if (instanceof<VpnTCB>(cb.get()) || instanceof<VpnUCB>(cb.get())) {
        _L4->onSessionConnectFromVPN(cb->id(), cb->key().srcAddress(), cb->key().dstAddress());
    } else if (instanceof<L3::MeshEnetRUCB>(cb.get()) || instanceof<MeshUCB>(cb.get())) {
        _L4->onSessionConnectFromMesh(cb->id(), cb->key().dstAddress());
    } else if (instanceof<ProxyTCB>(cb.get()) || instanceof<ProxyUCB>(cb.get())) {
        _L4->onSessionConnectFromProxy(cb->id(), cb->key().dstAddress());
    }
}

void L3TransportLayer::onSessionClosed(std::shared_ptr<ControlBlock> cb, bool force) {
    // Close proxy link when proxy session closed
    if (instanceof<ProxyControlBlock>(cb.get())) {
        // Proxy channel id is the source port in control block key.
        _L2->close(cb->key().srcAddress().port());
    }

    _L4->onSessionClose(cb->id(), force);
}

void
L3TransportLayer::onReadFromPacketProcessor(std::shared_ptr<ControlBlock> cb,
                                            NetworkBufferPointer buffer) {
    _L4->onReadFromSession(cb->id(), buffer);
}

void L3TransportLayer::connectFromPacketProcessor(Anyfi::Address &addr,
                                                  std::function<void(
                                                          unsigned short channelId)> callback) {
    _L2->connect(addr, [callback](unsigned short channelId) {
        callback(channelId);
    });
}

bool L3TransportLayer::writeFromPacketProcessor(Anyfi::Address &addr,
                                                NetworkBufferPointer buffer) {
    return _L2->write(addr, buffer);
}

void L3TransportLayer::writeFromPacketProcessor(unsigned short channelId,
                                                NetworkBufferPointer buffer) {
    _L2->write(channelId, buffer);
}

uint16_t L3TransportLayer::assignPort() {
    return _assignUnusedPort();
}

void L3TransportLayer::releasePort(uint16_t port) {
    _releasePort(port);
}
//
// Override interfaces : L3::IL3ForPacketProcessor
// ========================================================================================
//


//
// ========================================================================================
// Control Blocks
//
inline void
L3TransportLayer::_addControlBlock(std::shared_ptr<ControlBlock> cb, ControlBlockKey key) {
    Anyfi::Config::lock_type guard(_cbMapMutex);
    _cbMap[key] = cb;
}

inline void L3TransportLayer::_removeControlBlock(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_cbMapMutex);

    _removeControlBlockWithoutLock(key);
}

void L3TransportLayer::_removeControlBlockWithoutLock(ControlBlockKey &key) {
    auto cb = _findControlBlockWithoutLock(key);
    _cbMap.erase(key);
}

inline std::shared_ptr<ControlBlock> L3TransportLayer::_findControlBlock(unsigned short id) {
    Anyfi::Config::lock_type guard(_cbMapMutex);

    return _findControlBlockWithoutLock(id);
}

inline std::shared_ptr<ControlBlock> L3TransportLayer::_findControlBlockWithoutLock(unsigned short id) {
    for (const auto &cb : _cbMap)
        if (cb.second->id() == id)
            return cb.second;

    return nullptr;
}

inline std::shared_ptr<ControlBlock> L3TransportLayer::_findControlBlock(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_cbMapMutex);

    return _findControlBlockWithoutLock(key);
}

std::shared_ptr<ControlBlock> L3TransportLayer::_findControlBlockWithoutLock(ControlBlockKey &key) {
    for (const auto &cb : _cbMap) {
        if (cb.first == key) {
#ifdef NDEBUG
#else
            if (cb.second == nullptr) {
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3, "L3", "found control block, but cb.second is nullptr");
            }
#endif
            return cb.second;
        }
    }
    return nullptr;
}

std::list<std::shared_ptr<ControlBlock>> L3TransportLayer::_findControlBlocksWithDestAddr(Anyfi::Address destAddr) {
    auto cbs = std::list<std::shared_ptr<ControlBlock>>();

    Anyfi::Config::lock_type guard(_cbMapMutex);
    for (const auto &cb : _cbMap) {
        if (cb.first.dstAddress().type() == destAddr.type() && cb.first.dstAddress().addr() == destAddr.addr()) {
            cbs.push_front(cb.second);
        }
    }
    return cbs;
}

unsigned short L3TransportLayer::_getUnassignedCbId() {
    Anyfi::Config::lock_type guard(_cbMapMutex);

    unsigned short id = 0;
    while (id == 0 || _findControlBlockWithoutLock(id) != nullptr) {
        id = (unsigned short) (std::rand() % USHRT_MAX);
    }
    return id;
}

ControlBlockKey *L3TransportLayer::_getCBKey(unsigned short id) {
    Anyfi::Config::lock_type guard(_cbMapMutex);

    for (auto &iter : _cbMap)
        if (iter.second->id() == id)
            return const_cast<ControlBlockKey *>(&iter.first);

    return nullptr;
}
//
// Control Blocks
// ========================================================================================
//


//
// ========================================================================================
// Transport Ports
//
uint16_t L3TransportLayer::_assignUnusedPort() {
    Anyfi::Config::lock_type guard(_portMutex);

    for (uint16_t i = PORT_NUM_MAX_DEFINED_PORT + 1; i < UINT16_MAX; ++i) {
        if (!_ports[i]) {
            _ports[i] = true;
            return i;
        }
    }
    return 0;
}

void L3TransportLayer::_releasePort(uint16_t port) {
    Anyfi::Config::lock_type guard(_portMutex);

    _ports[port] = false;
}
//
// Transport Ports
// ========================================================================================
//

//
// L3TransportLayer Private Methods
// ========================================================================================
//
void L3TransportLayer::dump() {
#ifdef NDEBUG
#else
    {
        Anyfi::Config::lock_type guard(_portMutex);
        size_t numPorts = 0;
        for (uint16_t i = PORT_NUM_MAX_DEFINED_PORT + 1; i < UINT16_MAX; ++i) {
            if (_ports[i]) {
                numPorts++;
            }
        }
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3, "L3TransportLayer::dump",
                          "dumpPorts numPorts=" + to_string(numPorts));
    }
    {
        Anyfi::Config::lock_type guard(_cbMapMutex);
        size_t numCBs = _cbMap.size();
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3, "L3TransportLayer::dump",
                          "dumpCBs numCBs=" + to_string(numCBs));
        int vpnUCBs = 0;
        int vpnTCBs = 0;
        int p2pUCBs = 0;
        int p2pRUCBs = 0;
        int proxyUCBs = 0;
        int proxyTCBs = 0;
        for (auto &iter : _cbMap) {
            if (iter.first.srcAddress().connectionType() == Anyfi::ConnectionType::VPN) {
                if (iter.first.srcAddress().connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
                    vpnUCBs++;
                } else if (iter.first.srcAddress().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
                    vpnTCBs++;
                }
            } else if (iter.first.srcAddress().connectionType() == Anyfi::ConnectionType::WifiP2P) {
                if (iter.first.srcAddress().connectionProtocol() == Anyfi::ConnectionProtocol::MeshUDP) {
                    p2pUCBs++;
                } else if (iter.first.srcAddress().connectionProtocol() == Anyfi::ConnectionProtocol::MeshRUDP) {
                    p2pRUCBs++;
                }
            } else if (iter.first.srcAddress().connectionType() == Anyfi::ConnectionType::Proxy) {
                if (iter.first.dstAddress().connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
                    proxyUCBs++;
                } else if (iter.first.dstAddress().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
                    proxyTCBs++;
                }
            }
        }
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3, "L3TransportLayer::dump",
                          "dumpCBs vpnUCBs=" + to_string(vpnUCBs) +
                          " vpnTCBs=" + to_string(vpnTCBs) +
                          " p2pUCBs=" + to_string(p2pUCBs) +
                          " p2pRUCBs=" + to_string(p2pRUCBs) +
                          " proxyUCBs=" + to_string(proxyUCBs) +
                                  " proxyTCBs=" + to_string(proxyTCBs));
    }
#endif
}
