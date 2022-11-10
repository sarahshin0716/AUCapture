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

#include "ProxyChannel.h"

#define PROXY_READ_LIMIT 65000

void ProxyChannel::connect(Anyfi::Address address, const std::function<void(bool result)> &callback) {
    if (address.connectionType() == Anyfi::ConnectionType::VPN)
        throw std::invalid_argument("Invalid connection type");

    _address = address;
    _ipAddr = address.getIPv4AddrAsNum();
    _proxyLink = std::make_shared<SocketLink>(_selector, shared_from_this());
    _proxyLink->setReadLimit(PROXY_READ_LIMIT);
    _link = _proxyLink;

    state(CONNECTING);
    _proxyLink->connect(address, [this, callback](bool success) {
        updateTCPInfo();

        if (success) {
            state(CONNECTED);
        } else {
            state(CLOSED);
        }

        if (callback)
            callback(success);
    });
}

bool ProxyChannel::open(Anyfi::Address address) {
    _address = address;
    _ipAddr = address.getIPv4AddrAsNum();
    _proxyLink = std::make_shared<SocketLink>(_selector, shared_from_this());
    _proxyLink->setReadLimit(PROXY_READ_LIMIT);
    _link = _proxyLink;

    auto result = _proxyLink->open(address);
    if (result) {
        state(CONNECTED);
        auto assignedAddr = Anyfi::Socket::getSockAddress(_link->sock());
        _address.port(assignedAddr.port());
        return true;
    }
    return false;
}

void ProxyChannel::close() {
    state(CLOSED);
    if (_proxyLink) {
        _proxyLink->closeWithoutNotify();
        _proxyLink = nullptr;
    }
    _link = nullptr;
}

void ProxyChannel::write(NetworkBufferPointer buffer) {
    if (_link->state() != LinkState::CLOSED) {
        _proxyLink->write(buffer);

        // Track TCP Performance
        if (_link->addr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            setLastWriteTime(std::chrono::system_clock::now());
        // Track UDP DNS Performance
        } else if (_link->addr().connectionProtocol() == Anyfi::ConnectionProtocol::UDP && _link->addr().port() == Anyfi::TransmissionProtocol::DNS) {
            setLastWriteTime(std::chrono::system_clock::now());
        }
    }
}

void ProxyChannel::onClose(std::shared_ptr<Link> link) {
    state(CLOSED);
    if (_closeCallback)
        _closeCallback(shared_from_this());

    _proxyLink = nullptr;
    _link = nullptr;
}

void ProxyChannel::updateTCPInfo() {
    std::shared_ptr<tcp_info> tcpInfo = getTcpInfo();
    if (tcpInfo != nullptr) {
        if (setRtt(tcpInfo->tcpi_rtt) || setRttVar(tcpInfo->tcpi_rttvar)) {
            // Round trip time changed. Ack must have been received
            clearLastWriteTime();
        }
    }
}

std::shared_ptr<tcp_info> ProxyChannel::getTcpInfo() {
    if (addr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
        tcp_info info;
        socklen_t tcp_info_length = sizeof(info);
        auto link = _proxyLink;
        if (link != nullptr && getsockopt(link->sock(), SOL_TCP, TCP_INFO, &info, &tcp_info_length) >= 0) {
            return std::make_shared<tcp_info>(info);
        }
    }
    return nullptr;
}
