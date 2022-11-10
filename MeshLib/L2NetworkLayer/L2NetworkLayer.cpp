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

#include "L2NetworkLayer.h"
#include "Log/Frontend/Log.h"

L2NetworkLayer::L2NetworkLayer(Anyfi::UUID myUid, const std::string &myDeviceName) {
    _meshNetwork = std::make_shared<L2::MeshNetwork>(myUid, myDeviceName);
    _proxyNetwork = std::make_shared<L2::ProxyNetwork>();
    _vPNetwork = std::make_shared<L2::VPNetwork>();
    _connectCallbackMap.clear();
}

void L2NetworkLayer::initialize() {
    _meshNetwork->attachL2Interface(shared_from_this());
    _proxyNetwork->attachL2Interface(shared_from_this());
    _vPNetwork->attachL2Interface(shared_from_this());
    _meshNetwork->initTimer();
}

void L2NetworkLayer::terminate() {
    _meshNetwork->clean();
    _meshNetwork = nullptr;
    _proxyNetwork = nullptr;
    _vPNetwork = nullptr;
}

void L2NetworkLayer::onChannelAccepted(const ChannelInfo &channelInfo) {
    _meshNetwork->onChannelAccepted(channelInfo);
}

void L2NetworkLayer::onReadFromChannel(unsigned short channelId, const std::shared_ptr<Link> &link, NetworkBufferPointer buffer) {
    if (_meshNetwork->isContained(channelId)) {
        _meshNetwork->onRead(channelId, link, buffer);
    }else if (_vPNetwork->isContained(channelId)) {
        _vPNetwork->onRead(channelId, link, buffer);
    }else if (_proxyNetwork->isContained(channelId)) {
        _proxyNetwork->onRead(channelId, link, buffer);
    }else {
        Anyfi::Log::error( __func__, " Unknown channel id: " + to_string(channelId));
        _L1->closeChannel(channelId);
    }
}

void L2NetworkLayer::onClosed(unsigned short channelId) {
    if (_meshNetwork->isContained(channelId)) {
        _meshNetwork->onClose(channelId);
    } else if (_vPNetwork->isContained(channelId)) {
        _vPNetwork->onClose(channelId);
    } else if (_proxyNetwork->isContained(channelId)) {
        _proxyNetwork->onClose(channelId);
    }
}

bool L2NetworkLayer::write(const Anyfi::Address &address, NetworkBufferPointer buffer) {
    if (address.type() == Anyfi::AddrType::IPv4 || address.type() == Anyfi::AddrType::IPv6) {
        if (address.connectionType() == Anyfi::ConnectionType::Proxy) {
            throw std::runtime_error("Unsupported method");
        } else if (address.connectionType() == Anyfi::ConnectionType::VPN) {
            _vPNetwork->write(address, buffer);
            return true;
        }
    } else if (address.type() == Anyfi::AddrType::MESH_UID) {
        auto destUid = address.getMeshUID();
        if (!destUid.isEmpty()) {
            return _meshNetwork->write(destUid, buffer);
        }
    }
    return false;
}

void L2NetworkLayer::write(unsigned short channelId, NetworkBufferPointer buffer) {
#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_PROXY, __func__, "Proxy Data: write to " + to_string(channelId) + ", buffer size: " + to_string(buffer->size()));
#endif

    _L1->writeToChannel(channelId, buffer);
}


void L2NetworkLayer::openVPN() {
    auto vpnChannelInfo = _L1->openExternalVpnChannel();
    if (vpnChannelInfo.channelId != 0) {
        _vPNetwork->setVPNLink(std::make_shared<ChannelInfo>(vpnChannelInfo.channelId, vpnChannelInfo.addr));
    }
}

void L2NetworkLayer::openVPN(int fd) {
    auto vpnChannelInfo = _L1->openInternalVpnChannel(fd);
    if (vpnChannelInfo.channelId != 0) {
        _vPNetwork->setVPNLink(std::make_shared<ChannelInfo>(vpnChannelInfo.channelId, vpnChannelInfo.addr));
    }
}

void L2NetworkLayer::stopVPN() {
    auto vpnChannelInfo = _vPNetwork->getVPNChannelInfo();
    if (vpnChannelInfo != nullptr) {
        _L1->closeChannel(vpnChannelInfo->channelId);
        _vPNetwork->setVPNLink(nullptr);
    }
}



void L2NetworkLayer::connect(Anyfi::Address addr,
                             const std::function<void(unsigned short channelId)> &callback) {
    if (addr.isP2PConnectionType()) {
        throw std::invalid_argument("Connection type must NOT be P2P");
    }
    if (addr.connectionType() == Anyfi::ConnectionType::Proxy) {
        _proxyNetwork->connect(addr, callback);
    }else if (addr.connectionType() == Anyfi::ConnectionType::BluetoothP2P ||
            addr.connectionType() == Anyfi::ConnectionType::WifiP2P) {
    }

}

void L2NetworkLayer::connectMesh(Anyfi::Address addr, const std::function<void(Anyfi::Address meshUid)> &callback) {
    if (!addr.isP2PConnectionType()) {
        throw std::invalid_argument("Connection type must be P2P");
    }
    _addMeshConnectCallbackMap(addr, callback);
    _L1->connectChannel(addr, [this, callback](ChannelInfo linkInfo) {
        if (linkInfo.channelId == 0) {
            callback(Anyfi::Address());
            MeshCallbackPair callbackPair;
            _removeMeshConnectCallbackMap(linkInfo.addr, &callbackPair);
        } else {
            _meshNetwork->onChannelConnected(linkInfo);
        }
    });
}

Anyfi::Address L2NetworkLayer::open(Anyfi::Address networkAddr) {
    // MARK: MeshNetwork 생성 이벤트

    auto newLinkInfo = _L1->openChannel(networkAddr);

    _addAddressLinkMap({newLinkInfo.addr, newLinkInfo.channelId});

    return newLinkInfo.addr;
}

void L2NetworkLayer::close(Anyfi::Address addr) {
    if (addr.type() == Anyfi::AddrType::MESH_UID) {
        _meshNetwork->close(addr.getMeshUID());
    }else {
        std::pair<Anyfi::Address, unsigned short> addressPair;
        if (_removeAddressLinkMap(addr, &addressPair)) {
            _L1->closeChannel(addressPair.second);
        }

        MeshCallbackPair callbackPair;
        if (_removeMeshConnectCallbackMap(addr, &callbackPair)) {
            callbackPair.second(Anyfi::Address());
        }
    }
}
void L2NetworkLayer::close(unsigned short channelId) {
    if (_proxyNetwork->isContained(channelId)) {
        _proxyNetwork->close(channelId);
        _L1->closeChannel(channelId);
    } else if (_meshNetwork->isContained(channelId)) {
//        _meshNetwork->close(channelId);
        Anyfi::Log::error(__func__, "L2 Close Mesh Channel: " + to_string(channelId));
    }
}

void L2NetworkLayer::setMeshNodeType(uint8_t type) {
    _meshNetwork->setMeshNodeType(type);
}
std::pair<Anyfi::Address, uint8_t> L2NetworkLayer::getClosestMeshNode(uint8_t type) {
    return _meshNetwork->getClosestMeshNode(type);
};

void L2NetworkLayer::writeToChannel(unsigned short channelId, NetworkBufferPointer buffer) {
    _L1->writeToChannel(channelId, buffer);
}

void L2NetworkLayer::onChannelClosed(unsigned short channelId, Anyfi::Address addr) {
    _L3->onChannelClosed(channelId, addr);
}

void L2NetworkLayer::connectChannel(Anyfi::Address addr, const std::function<void(unsigned short channelId)> &callback) {
    _L1->connectChannel(addr, [callback](ChannelInfo linkInfo) {
        callback(linkInfo.channelId);
    });
}

void L2NetworkLayer::onMeshConnected(std::shared_ptr<ChannelInfo> channelInfo, Anyfi::UUID uuid) {
    MeshCallbackPair callbackPair;
    if (_removeMeshConnectCallbackMap(channelInfo->addr, &callbackPair)) {
        auto uuidAddr = Anyfi::Address();
        uuidAddr.setMeshUID(uuid);
        uuidAddr.connectionType(channelInfo->addr.connectionType());
        callbackPair.second(uuidAddr);
    }else {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "Mesh State: onMeshConnected. but callback already removed");
#endif
    }
}
void L2NetworkLayer::onMeshConnectFail(std::shared_ptr<ChannelInfo> channelInfo) {
    closeNeighborChannel(channelInfo->channelId);
    MeshCallbackPair callbackPair;
    if (_removeMeshConnectCallbackMap(channelInfo->addr, &callbackPair)) {
        callbackPair.second(Anyfi::Address());
    }
}

void L2NetworkLayer::onReadMeshPacketFromNetwork(Anyfi::Address srcAddr, NetworkBufferPointer buffer) {
    _L3->onReadFromMesh(srcAddr, buffer);
}

void L2NetworkLayer::closeNeighborChannel(unsigned short channelId) {
    Anyfi::Log::error(__func__, "L2 call L1 closeChannel: " + to_string(channelId));
    _L1->closeChannel(channelId);
}
void L2NetworkLayer::onMeshNodeClosed(std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses) {
    _L3->onMeshNodeClosed(addresses);
}

void L2NetworkLayer::onReadIPPacketFromVPN(NetworkBufferPointer buffer) {
    _L3->onReadFromVPN(buffer);
}

void L2NetworkLayer::onReadIPPacketFromProxy(Anyfi::Address addr, NetworkBufferPointer buffer, unsigned short channelId) {
    _L3->onReadFromProxy(addr, buffer, channelId);
}

void L2NetworkLayer::_addAddressLinkMap(std::pair<Anyfi::Address, unsigned short> addressPair) {
    Anyfi::Config::lock_type guard(_addressLinkMapMutex);

    _addressLinkMap.insert(addressPair);
}
bool L2NetworkLayer::_removeAddressLinkMap(Anyfi::Address addr, std::pair<Anyfi::Address, unsigned short> *pair) {
    Anyfi::Config::lock_type guard(_addressLinkMapMutex);

    auto it = _addressLinkMap.find(addr);
    if (it != _addressLinkMap.end()) {
        *pair = *it;
        _addressLinkMap.erase(it);
        return true;
    }
    return false;
}

void L2NetworkLayer::_addMeshConnectCallbackMap(Anyfi::Address addr,
                                                const std::function<void(Anyfi::Address meshUid)> &callback) {
    Anyfi::Config::lock_type guard(_connectCallbackMapMutex);

    _connectCallbackMap[addr] = callback;
}

bool L2NetworkLayer::_removeMeshConnectCallbackMap(Anyfi::Address addr,
                                                   std::pair<Anyfi::Address, std::function<void(
                                                           Anyfi::Address meshUid)>> *pair) {
    Anyfi::Config::lock_type guard(_connectCallbackMapMutex);

    auto it = _connectCallbackMap.find(addr);
    if (it != _connectCallbackMap.end()) {
        *pair = *it;
        _connectCallbackMap.erase(it);
        return true;
    }
    return false;
}

