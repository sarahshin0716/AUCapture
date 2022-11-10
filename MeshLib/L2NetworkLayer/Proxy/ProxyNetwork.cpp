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

#include "ProxyNetwork.h"
#include "Log/Frontend/Log.h"

L2::ProxyNetwork::ProxyNetwork() {
    _proxyMap.clear();
}

bool L2::ProxyNetwork::isContained(unsigned short channelId) {
    Anyfi::Config::lock_type guard(_proxyMapMutex);

    auto proxyMapIt = _proxyMap.find(channelId);
    return proxyMapIt != _proxyMap.end();
}

void L2::ProxyNetwork::onRead(unsigned short channelId, const std::shared_ptr<Link>& link, NetworkBufferPointer buffer) {
    auto proxy = _getProxy(channelId);
    if (proxy != nullptr && proxy->linkState() == LinkState::CONNECTED) {
        _L2->onReadIPPacketFromProxy(proxy->dstAddr(), buffer, proxy->channelId());
    }
}

void L2::ProxyNetwork::write(Anyfi::Address dstAddr, NetworkBufferPointer buffer) {
    throw std::runtime_error("Unsupported method");
}

void L2::ProxyNetwork::write(unsigned short channelId, NetworkBufferPointer buffer) {
    auto proxy = _getProxy(channelId);
    if (proxy != nullptr && proxy->linkState() == LinkState::CONNECTED) {
        _L2->writeToChannel(proxy->channelId(), buffer);
    }
}

void L2::ProxyNetwork::onClose(unsigned short channelId) {
    auto proxy = _getProxy(channelId);
    if (proxy != nullptr) {
        proxy->linkState(LinkState::CLOSED);
        _L2->onChannelClosed(channelId, proxy->dstAddr());
        _removeProxy(channelId);
    }
}

void L2::ProxyNetwork::connect(Anyfi::Address addr, const std::function<void(unsigned short channelId)> &callback) {
    auto connectedCallback = [this, addr, callback](unsigned short channelId) {
        if (channelId != 0) {
            auto newProxy = _addProxy(channelId, addr);
            newProxy->linkState(LinkState::CONNECTED);
            callback(channelId);
        }else {
            callback(0);
        }
    };
    _L2->connectChannel(addr, connectedCallback);
}
void L2::ProxyNetwork::close(unsigned short channelId) {
    auto proxy = _getProxy(channelId);
    if (proxy != nullptr) {
        proxy->linkState(LinkState::CLOSED);
        _removeProxy(channelId);
    }
}

void L2::ProxyNetwork::onLinkStateUpdated(unsigned short channelId, LinkState linkState) {
    auto proxy = _getProxy(channelId);
    if (proxy != nullptr) {
        proxy->linkState(linkState);
    }
}

std::shared_ptr<L2::Proxy> L2::ProxyNetwork::_getProxy(unsigned short channelId) {
    Anyfi::Config::lock_type guard(_proxyMapMutex);

    auto proxyMapIt = _proxyMap.find(channelId);
    if (proxyMapIt == _proxyMap.end()) {
        return nullptr;
    }else {
        return proxyMapIt->second;
    }
}

std::shared_ptr<L2::Proxy> L2::ProxyNetwork::_getProxy(Anyfi::Address dstAddr) {
    Anyfi::Config::lock_type guard(_proxyMapMutex);

    auto foundProxy = std::find_if(_proxyMap.begin(), _proxyMap.end(), [dstAddr](const std::pair<unsigned short, std::shared_ptr<L2::Proxy>> &proxyPair) {
        return (proxyPair.second->dstAddr().type() == dstAddr.type() && proxyPair.second->dstAddr().addr() == dstAddr.addr());
    });
    if (foundProxy == _proxyMap.end()) {
        return nullptr;
    }else {
        return foundProxy->second;
    }
}

std::shared_ptr<L2::Proxy> L2::ProxyNetwork::_addProxy(unsigned short channelId, const Anyfi::Address &dstAddr) {
    Anyfi::Config::lock_type guard(_proxyMapMutex);

    auto proxy = std::make_shared<L2::Proxy>();
    proxy->channelId(channelId);
    proxy->dstAddr(dstAddr);
    _proxyMap[channelId] = proxy;
    return proxy;
}

void L2::ProxyNetwork::_removeProxy(unsigned short channelId) {
    Anyfi::Config::lock_type guard(_proxyMapMutex);

    auto proxyMapIt = _proxyMap.find(channelId);
    if (proxyMapIt != _proxyMap.end()) {
        _proxyMap.erase(proxyMapIt);
    }
}
