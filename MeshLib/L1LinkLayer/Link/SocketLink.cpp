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

#include "SocketLink.h"

#include "../../Log/Frontend/Log.h"
#include "../../Common/Network/Buffer/NetworkBufferPool.h"

Anyfi::SocketConnectResult SocketLink::doSockConnect(Anyfi::Address address, const std::string &ifname) {
    if (address.connectionType() == Anyfi::ConnectionType::VPN)
        throw std::invalid_argument("Not allowed VPN connection type.");

    _sock = Anyfi::Socket::create(address, false);
    protectFromVPN();

    return Anyfi::Socket::connect(_sock, address, ifname);
}

bool SocketLink::finishSockConnect() {
    bool result = Anyfi::Socket::connectResult(_sock) == Anyfi::SocketConnectResult::CONNECTED;
    if (!result) {
        _sock = -1;
    }
    onSockConnectFinished(result);
    return result;
}

bool SocketLink::doOpen(Anyfi::Address address, const std::string& in) {
    if (address.connectionType() == Anyfi::ConnectionType::VPN)
        throw std::invalid_argument("Not allowed VPN connection type.");

    _sock = Anyfi::Socket::create(address, false);
    protectFromVPN();

    return Anyfi::Socket::open(_sock, address, in);
}

std::shared_ptr<Link> SocketLink::doAccept() {
    int newSock = Anyfi::Socket::accept(_sock);
    if (newSock <= 0) {
        return nullptr;
    }
    return std::shared_ptr<SocketLink>(new SocketLink(newSock, _selector));
}

void SocketLink::doClose() {
    Anyfi::Socket::close(_sock);

    _sock = -1;
}

NetworkBufferPointer SocketLink::doRead() {
    return _readFromSock(_sock,_readLimit);
}

int SocketLink::doWrite(NetworkBufferPointer buffer) {
    return Anyfi::Socket::write(_sock, buffer);
}
