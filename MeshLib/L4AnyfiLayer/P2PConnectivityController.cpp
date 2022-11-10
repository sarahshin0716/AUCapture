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

#include "P2PConnectivityController.h"
#include "IL4AnyfiLayer.h"
#include "Log/Backend/LogManager.h"

#include <mutex>


L4::P2PConnectivityController::P2PConnectivityController() {
    _serverAddrMap.clear();
    _linkStateMap.clear();
    _linkAddrMap.clear();
}

void L4::P2PConnectivityController::startAcceptP2P(bool accept, int quality){
    Anyfi::Config::lock_type acceptGuard(_acceptMutex);

    _acceptP2P = accept;
    _acceptQuality = quality;

    if (accept) {
        std::set<Anyfi::P2PType> acceptableP2P = _Connectivity->getAcceptableP2P();
        for (const auto &p2pType : acceptableP2P){
            if(!_isServerOpened(p2pType)){
                _Connectivity->openP2PGroup(p2pType, std::bind(&L4::P2PConnectivityController::_onP2PGroupOpened, this, std::placeholders::_1, std::placeholders::_2));
            }
        }
    } else {
        auto serverAddrMap = _getServerAddressMap();
        _clearServerAddressMap();
        for (const auto &item : serverAddrMap){
            _L3->closeServerLink(item.second);
            _Connectivity->closeP2PGroup(item.first);
        }
    }
}

void L4::P2PConnectivityController::_onP2PGroupOpened(Anyfi::P2PType p2pType, Anyfi::Address address) {
    Anyfi::Config::lock_type guard(_acceptMutex);
    if (_acceptP2P) {
        // Check open P2P group failure
        if (!address.isEmpty()) {
            auto serverAddr = Anyfi::Address();
            serverAddr.addr(address.addr());
            serverAddr.type(Anyfi::AddrType::IPv4);
            serverAddr.connectionType(Anyfi::ConnectionType::WifiP2P);
            serverAddr.connectionProtocol(Anyfi::ConnectionProtocol::TCP);
            serverAddr.port(0);
            _L3->openServerLink(serverAddr, [this, p2pType](Anyfi::Address assignedAddr) {
                // MARK: 어짜피 _L3에서 비동기가 아닌 바로 콜백을 호출하기에, Lock guard 범위 이내임. 추후 비동기로 바꾼다면 Lock을 걸어줘야 함.
                // assignedAddr -> 할당받은 포트 정보가 담긴 addr
                // Check open server link failure
                if (assignedAddr.isEmpty()) {
                    _Connectivity->closeP2PGroup(p2pType);
                } else {
                    if (_acceptP2P) {
                        _serverAddrMap.insert({p2pType, assignedAddr});

                        _Connectivity->onP2PServerOpened(assignedAddr, _acceptQuality);
                    } else {
                        _L3->closeServerLink(assignedAddr);
                        _Connectivity->closeP2PGroup(p2pType);
                    }
                }
            });
        }
    } else {
        _Connectivity->closeP2PGroup(p2pType);
    }
}

void L4::P2PConnectivityController::connectP2P(Anyfi::Address addr, const std::function<void(Anyfi::Address addr)> &callback) {
    _L3->connectMeshLink(addr, [this, addr, callback](Anyfi::Address address) {
        // Check connect mesh link failure
        if (address.isEmpty()) {
            callback(address);
        } else {
            addLink(addr, address, ConnectivityState::Connected);

            callback(address);
        }
    });
}

void L4::P2PConnectivityController::acceptP2P(Anyfi::Address addr) {
    Anyfi::Config::lock_type guard(_acceptMutex);

    if (_acceptP2P) {
        addLink(addr, ConnectivityState::Accepted);
    } else {
        _L3->closeMeshLink(addr);
    }
}

void L4::P2PConnectivityController::disconnectP2P(Anyfi::Address addr) {
    if (addr.isEmpty()){
        auto linkStateMap = _getLinkStateMap();
        _clearLinkStateMap();
        for (const auto &item : linkStateMap){
            if (item.second == ConnectivityState::Connected){
                _L3->closeMeshLink(item.first);
                Anyfi::Address p2pAddr = _getOtherLinkAddress(item.first);
                _Connectivity->disconnectP2P(p2pAddr);
            } else if (item.second == ConnectivityState::Accepted) {
                _L3->closeMeshLink(item.first);
            } else {
                std::invalid_argument("Unexpected ConnectivityState on disconnect all P2P");
            }
        }
    } else if (addr.type() == Anyfi::AddrType::IPv4 || addr.type() == Anyfi::AddrType::IPv6){
        // Close only if not removed yet
        Anyfi::Address nodeAddr = _getOtherLinkAddress(addr);
        if (!nodeAddr.isEmpty()) {
            removeLink(addr);
            _L3->closeMeshLink(nodeAddr);
        }

    } else {
        Anyfi::Address p2pAddr = _getOtherLinkAddress(addr);
        if (p2pAddr.isEmpty()) {
            // Accepted link closed or link already closed
            removeLink(addr);
        } else {
            // Connected link closed
            removeLink(addr);
            _Connectivity->disconnectP2P(p2pAddr);
        }
    }
    _Connectivity->onP2PDisconnect(addr);
}
void L4::P2PConnectivityController::disconnectAcceptedP2P(){
    std::vector<Anyfi::Address> acceptedLinkList = _getAcceptedLinkAddr();
    for (const auto &nodeAddr : acceptedLinkList) {
        removeLink(nodeAddr);
        _L3->closeMeshLink(nodeAddr);
    }
}

void L4::P2PConnectivityController::discoverP2PGroup(bool discover) {
    _Connectivity->discoverP2PGroup(discover);
}

bool L4::P2PConnectivityController::_isServerOpened(Anyfi::P2PType type) {
    auto serverAddr = _getServerAddress(type);
    return !serverAddr.isEmpty();
}

ConnectivityState L4::P2PConnectivityController::getLinkConnectionState(Anyfi::Address address) {
    auto linkStateMap = _getLinkStateMap();
    auto it = linkStateMap.find(address);
    if (it != linkStateMap.end()) {
        return (*it).second;
    }

    auto otherAddress = _getOtherLinkAddress(address);
    if (!otherAddress.isEmpty()) {
        it = linkStateMap.find(otherAddress);
        if (it != linkStateMap.end()) {
            return (*it).second;
        }
    }

    return ConnectivityState::Disconnected;
}

std::vector<Anyfi::Address> L4::P2PConnectivityController::_getAcceptedLinkAddr() {
    std::vector<Anyfi::Address> acceptedLinkList;
    auto linkStateMap = _getLinkStateMap();

    for (const auto &item : linkStateMap) {
        if (item.second == ConnectivityState::Accepted)
            acceptedLinkList.push_back(item.first);
    }

    return acceptedLinkList;
}

void L4::P2PConnectivityController::addLink(Anyfi::Address nodeAddr, ConnectivityState connectivityState) {
    _insertLinkState(nodeAddr, connectivityState);
}
void L4::P2PConnectivityController::addLink(Anyfi::Address p2pAddr, Anyfi::Address nodeAddr, ConnectivityState connectivityState) {
    _insertLinkState(nodeAddr, connectivityState);
    _insertLinkAddress(p2pAddr, nodeAddr);
}

void L4::P2PConnectivityController::removeLink(Anyfi::Address address) {
    auto otherAddress = _getOtherLinkAddress(address);
    _removeLinkAddress(address);
    _removeLinkState(address);

    if (!otherAddress.isEmpty()) {
        _removeLinkAddress(otherAddress);
        _removeLinkState(otherAddress);
    }
}

L4::P2PConnectivityController::LinkAddressMap L4::P2PConnectivityController::_getLinkAddressMap() {
    Anyfi::Config::lock_type guard(_linkAddrMutex);
    return _getLinkAddressMapWithoutLock();
}

L4::P2PConnectivityController::LinkAddressMap L4::P2PConnectivityController::_getLinkAddressMapWithoutLock() {
    return _linkAddrMap;
}

void L4::P2PConnectivityController::_insertLinkAddress(Anyfi::Address address,
                                                       Anyfi::Address otherAddress) {
    Anyfi::Config::lock_type guard(_linkAddrMutex);
    _insertLinkAddressWihtoutLock(address, otherAddress);
}
void L4::P2PConnectivityController::_insertLinkAddressWihtoutLock(Anyfi::Address address,
                                                                  Anyfi::Address otherAddress) {
    _linkAddrMap[address] = otherAddress;
    _linkAddrMap[otherAddress] = address;
}

Anyfi::Address L4::P2PConnectivityController::_getOtherLinkAddress(Anyfi::Address address) {
    Anyfi::Config::lock_type guard(_linkAddrMutex);
    return _getOtherLinkAddressWithoutLock(address);
}
Anyfi::Address L4::P2PConnectivityController::_getOtherLinkAddressWithoutLock(Anyfi::Address address) {
    auto it = _linkAddrMap.find(address);
    if (it == _linkAddrMap.end()) {
        return Anyfi::Address();
    } else {
        return (*it).second;
    }
}
void L4::P2PConnectivityController::_removeLinkAddress(Anyfi::Address address) {
    Anyfi::Config::lock_type guard(_linkAddrMutex);
    _removeLinkAddressWithoutLock(address);
}
void L4::P2PConnectivityController::_removeLinkAddressWithoutLock(Anyfi::Address address) {
    _linkAddrMap.erase(address);
}

L4::P2PConnectivityController::ServerAddressMap L4::P2PConnectivityController::_getServerAddressMap() {
    Anyfi::Config::lock_type guard(_serverAddrMutex);
    return _getServerAddressMapWithoutLock();
}

L4::P2PConnectivityController::ServerAddressMap L4::P2PConnectivityController::_getServerAddressMapWithoutLock() {
    return _serverAddrMap;
}

Anyfi::Address L4::P2PConnectivityController::_getServerAddress(Anyfi::P2PType type) {
    Anyfi::Config::lock_type guard(_serverAddrMutex);
    return _getServerAddressWithoutLock(type);
}

Anyfi::Address L4::P2PConnectivityController::_getServerAddressWithoutLock(Anyfi::P2PType type) {
    auto it = _serverAddrMap.find(type);
    if (it == _serverAddrMap.end()) {
        return Anyfi::Address();
    }else {
        return (*it).second;
    }
}

void L4::P2PConnectivityController::_clearServerAddressMap() {
    Anyfi::Config::lock_type guard(_serverAddrMutex);
    _clearServerAddressMapWithoutLock();
}

void L4::P2PConnectivityController::_clearServerAddressMapWithoutLock() {
    _serverAddrMap.clear();
}

L4::P2PConnectivityController::LinkStateMap L4::P2PConnectivityController::_getLinkStateMap() {
    Anyfi::Config::lock_type guard(_linkStateMutex);
    return _getLinkStateMapWithoutLock();
}

L4::P2PConnectivityController::LinkStateMap L4::P2PConnectivityController::_getLinkStateMapWithoutLock() {
    return _linkStateMap;
}

ConnectivityState L4::P2PConnectivityController::_getLinkState(Anyfi::Address address) {
    Anyfi::Config::lock_type guard(_linkStateMutex);
    return _getLinkStateWithoutLock(address);
}
ConnectivityState L4::P2PConnectivityController::_getLinkStateWithoutLock(Anyfi::Address address) {
    auto it = _linkStateMap.find(address);
    if (it == _linkStateMap.end()) {
        return ConnectivityState::Disconnected;
    }else {
        return (*it).second;
    }
}

void L4::P2PConnectivityController::_insertLinkState(Anyfi::Address address,
                                                     ConnectivityState state) {
    Anyfi::Config::lock_type guard(_linkStateMutex);
    _insertLinkStateWihtoutLock(address, state);
}
void L4::P2PConnectivityController::_insertLinkStateWihtoutLock(Anyfi::Address address,
                                                                ConnectivityState state) {
    _linkStateMap[address] = state;
}
void L4::P2PConnectivityController::_removeLinkState(Anyfi::Address address) {
    Anyfi::Config::lock_type guard(_linkStateMutex);
    _removeLinkStateWithoutLock(address);
}
void L4::P2PConnectivityController::_removeLinkStateWithoutLock(Anyfi::Address address) {
    _linkStateMap.erase(address);
}

void L4::P2PConnectivityController::_clearLinkStateMap() {
    Anyfi::Config::lock_type guard(_linkStateMutex);
    _clearLinkStateMapWithoutLock();
}

void L4::P2PConnectivityController::_clearLinkStateMapWithoutLock() {
    _linkStateMap.clear();
}
