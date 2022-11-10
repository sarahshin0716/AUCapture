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

#include "WifiP2PLink.h"
#include "Link.h"
#include "../Channel/Packet/P2PLinkHandshakePacket.h"
#include "../../Log/Frontend/Log.h"

#if defined(ANDROID) || defined(__ANDROID__)
#define UPLINK_INF_NAME     "p2p0"
#define DOWNLINK_INF_NAME   "wlan0"
#else
#define UPLINK_INF_NAME     "en1"
#define DOWNLINK_INF_NAME   "en1"
#endif // ifdef __ANDROID__

//
////
//// ========================================================================================
//// WifiP2PLink
////
//
//void WifiP2PLink::write(NetworkBufferPointer buffer) {
//    _writeUDPLink->write(buffer);
//}
//
//Anyfi::SocketConnectResult WifiP2PLink::doSockConnect(Anyfi::Address address) {
//    auto parent = std::dynamic_pointer_cast<WifiP2PLink>(shared_from_this());
//    _handshakeLink = std::shared_ptr<WifiP2PHandshakeLink>(new WifiP2PHandshakeLink(parent));
//    _handshakeLink->connect(address, [](bool){});
//
//    return Anyfi::SocketConnectResult::CONNECTING;
//}
//
//bool WifiP2PLink::finishSockConnect() {
//    return false;
//}
//
//LinkHandshakeResult WifiP2PLink::doHandshake() {
//    if (!_handshakeLink) {
//        Anyfi::Log::error(__func__, "_handshakeLink is nullptr");
//        return LinkHandshakeResult::HANDSHAKE_FAILED;
//    }
//
//    return _handshakeLink->doHandshake();
//}
//
//LinkHandshakeResult WifiP2PLink::processHandshake(NetworkBufferPointer buffer) {
//    return LinkHandshakeResult::HANDSHAKE_FAILED;
//}
//
//bool WifiP2PLink::checkHandshakeTimeout() {
//    return false;
//}
//
//bool WifiP2PLink::doOpen(Anyfi::Address address) {
//    return false;
//}
//
//std::shared_ptr<Link> WifiP2PLink::doAccept() {
//    return std::shared_ptr<Link>();
//}
//
//void WifiP2PLink::doClose() {
//    if (_readUDPLink)
//        _readUDPLink->close();
//    if (_writeUDPLink)
//        _writeUDPLink->close();
//    if (_handshakeLink)
//        _handshakeLink->close();
//}
//
//NetworkBufferPointer WifiP2PLink::doRead() {
//    return NetworkBufferPointer();
//}
//
//void WifiP2PLink::doWrite(NetworkBufferPointer buffer) {
//
//}
//
//Anyfi::Address WifiP2PLink::openReadLink() {
//    auto serverAddr = Anyfi::Address();
//    serverAddr.connectionProtocol(Anyfi::ConnectionProtocol::UDP);
//    serverAddr.type(Anyfi::AddrType::IPv6);
//    serverAddr.addr("::");
//    serverAddr.port(0);
//
//    _readUDPLink = std::shared_ptr<WifiP2PInternalLink>(new WifiP2PInternalLink(std::dynamic_pointer_cast<WifiP2PLink>(shared_from_this())));
//    _readUDPLink->open(serverAddr);
//
//    // return을 할 때는 주소를 받은 상대가 연결을 할 수 있도록, 포트와 주소가 명확하여야 한다.
//    auto assignedAddr = Anyfi::Socket::getAddress(_readUDPLink->_sock, Anyfi::AddrType::IPv6);
//
//    serverAddr.port(assignedAddr.port());
//    // 상대방에게 연결을 시도할 주소를 알려주기 위해 android p2p0의 IPv6 Address를 부여한다.
//    // TODO: 연결에 사용할 인터페이스 이름을 어딘가에서 지정해줘야 함.
//    std::string ifname = _isGroupOwner? UPLINK_INF_NAME : DOWNLINK_INF_NAME;
//    serverAddr.addr(Anyfi::Address::getIPv6AddrFromInterface(ifname).addr());
//    return serverAddr;
//}
//
//void WifiP2PLink::connectWriteLink(const Anyfi::Address &writeAddr) {
//    _writeUDPLink = std::shared_ptr<WifiP2PInternalLink>(new WifiP2PInternalLink(std::dynamic_pointer_cast<WifiP2PLink>(shared_from_this())));
//    _writeUDPLink->connect(writeAddr, [](bool result){});
//}
//
////
//// WifiP2PLink
//// ========================================================================================
////
//
//
////
//// ========================================================================================
//// WifiP2PInternalLink
////
//
//Anyfi::SocketConnectResult WifiP2PInternalLink::doSockConnect(Anyfi::Address address) {
//    if (address.connectionProtocol() != Anyfi::ConnectionProtocol::UDP) {
//        throw std::invalid_argument("Connection Protocol은 반드시 UDP여야 합니다.");
//    }
//    if (address.type() != Anyfi::AddrType::IPv6) {
//        throw std::invalid_argument("Address Type은 반드시 IPv6여야 합니다.");
//    }
//    _sock = Anyfi::Socket::create(address, false);
//    protectFromVPN(_sock);
//
//    std::string ifname = _parent->_isGroupOwner ? DOWNLINK_INF_NAME : UPLINK_INF_NAME;
//    return Anyfi::Socket::connect(_sock, address, ifname);
//}
//
//bool WifiP2PInternalLink::finishSockConnect() {
//    bool result = Anyfi::Socket::connectResult(_sock) == Anyfi::SocketConnectResult::CONNECTED;
//    onSockConnectFinished(result);
//    return result;
//}
//
//bool WifiP2PInternalLink::doOpen(Anyfi::Address address) {
//    if (address.connectionProtocol() != Anyfi::ConnectionProtocol::UDP) {
//        throw std::invalid_argument("Connection Protocol은 반드시 UDP여야 합니다.");
//    }
//    if (address.type() != Anyfi::AddrType::IPv6) {
//        throw std::invalid_argument("Address Type은 반드시 IPv6여야 합니다.");
//    }
//    _sock = Anyfi::Socket::create(address, false);
//    protectFromVPN(_sock);
//
//    return Anyfi::Socket::open(_sock, address);
//}
//
//std::shared_ptr<Link> WifiP2PInternalLink::doAccept() {
//    return std::shared_ptr<Link>();
//}
//
//void WifiP2PInternalLink::doClose() {
//    Anyfi::Socket::close(_sock);
//    _sock = -1;
//}
//
//NetworkBufferPointer WifiP2PInternalLink::doRead() {
//    if (_parent->_handshakeLink->state() != LinkState::CLOSED) {
//        _parent->_handshakeLink->close();
//    }
//    return _readFromSock(_sock);
//}
//
//void WifiP2PInternalLink::doWrite(NetworkBufferPointer buffer) {
//    Anyfi::Socket::write(_sock, buffer);
//}
//
////
//// WifiP2PInternalLink
//// ========================================================================================
////
//
//
////
//// ========================================================================================
//// WifiP2PExternalLink
////
//
//Anyfi::SocketConnectResult WifiP2PHandshakeLink::doSockConnect(Anyfi::Address address) {
//    if (address.connectionProtocol() != Anyfi::ConnectionProtocol::TCP) {
//        throw std::invalid_argument("Connection Protocol must be TCP");
//    }
//
//    _sock = Anyfi::Socket::create(address, false);
//    protectFromVPN(_sock);
//
//    return Anyfi::Socket::connect(_sock, address);
//}
//
//bool WifiP2PHandshakeLink::finishSockConnect() {
//    bool result = Anyfi::Socket::connectResult(_sock) == Anyfi::SocketConnectResult::CONNECTED;
//    onSockConnectFinished(result);
//    return result;
//}
//
//LinkHandshakeResult WifiP2PHandshakeLink::doHandshake() {
//    auto parent = std::dynamic_pointer_cast<WifiP2PLink>(_parent);
//    if (parent->isGroupOwner()) {
//        registerOperation(_selector, SelectionKeyOp::OP_READ);
//    }else {
//        auto readAddr = parent->openReadLink();
//        sendHandshakeREQ(readAddr);
//    }
//    parent->state(state());
//    return LinkHandshakeResult::HANDSHAKING;
//}
//
//LinkHandshakeResult WifiP2PHandshakeLink::processHandshake(NetworkBufferPointer buffer) {
//    auto parent = std::dynamic_pointer_cast<WifiP2PLink>(_parent);
//
//    state(LinkState::CONNECTED);
//    parent->state(state());
//
//    auto packetType = P2PLinkHandshakePacket::getType(buffer);
//    if (packetType == P2PLinkHandshakePacket::REQ) {
//        processHandshakeREQ(buffer);
//        return LinkHandshakeResult::ACCEPT_SUCCESS;
//    }else if (packetType == P2PLinkHandshakePacket::RES) {
//        processHandshakeRES(buffer);
//        return LinkHandshakeResult::CONNECT_SUCCESS;
//    }else {
//        throw std::runtime_error("Invalid Packet type");
//    }
//
//}
//
//bool WifiP2PHandshakeLink::checkHandshakeTimeout() {
//    // TODO: Timeout 구현
//    return false;
//}
//
//bool WifiP2PHandshakeLink::doOpen(Anyfi::Address address) {
//    _sock = Anyfi::Socket::create(address, false);
//    protectFromVPN(_sock);
//
//    return Anyfi::Socket::open(_sock, address);
//}
//
//std::shared_ptr<Link> WifiP2PHandshakeLink::doAccept() {
//    return std::shared_ptr<Link>();
//}
//
//void WifiP2PHandshakeLink::doClose() {
//    Anyfi::Socket::close(_sock);
//    _sock = -1;
//}
//
//NetworkBufferPointer WifiP2PHandshakeLink::doRead() {
//    return _readFromSock(_sock);
//}
//
//void WifiP2PHandshakeLink::doWrite(NetworkBufferPointer buffer) {
//    Anyfi::Socket::write(_sock, buffer);
//}
//
//void WifiP2PHandshakeLink::sendHandshakeREQ(Anyfi::Address address) {
//    auto handshakeReqPacket = std::make_shared<P2PLinkHandshakePacket>(P2PLinkHandshakePacket::Type::REQ, address);
//    write(handshakeReqPacket->toPacket());
//}
//
//void WifiP2PHandshakeLink::sendHandshakeRES(Anyfi::Address address) {
//    auto handshakeResPacket = std::make_shared<P2PLinkHandshakePacket>(P2PLinkHandshakePacket::Type::RES, address);
//    write(handshakeResPacket->toPacket());
//}
//
//void WifiP2PHandshakeLink::processHandshakeREQ(NetworkBufferPointer buffer) {
//    auto parent = std::dynamic_pointer_cast<WifiP2PLink>(_parent);
//
//    auto handshakePacket = std::make_shared<P2PLinkHandshakePacket>(buffer);
//    auto writeAddr = handshakePacket->addr();
//    writeAddr.connectionProtocol(Anyfi::ConnectionProtocol::UDP);
//    writeAddr.connectionType(Anyfi::ConnectionType::WifiP2P);
//    parent->connectWriteLink(writeAddr);
//
//    auto readAddr = parent->openReadLink();
//
//    sendHandshakeRES(readAddr);
//}
//
//void WifiP2PHandshakeLink::processHandshakeRES(NetworkBufferPointer buffer) {
//    auto parent = std::dynamic_pointer_cast<WifiP2PLink>(_parent);
//
//    auto handshakePacket = std::make_shared<P2PLinkHandshakePacket>(buffer);
//    auto writeAddr = handshakePacket->addr();
//    writeAddr.connectionProtocol(Anyfi::ConnectionProtocol::UDP);
//    writeAddr.connectionType(Anyfi::ConnectionType::WifiP2P);
//    parent->connectWriteLink(writeAddr);
//}
//
////
//// WifiP2PExternalLink
//// ========================================================================================
////
//
//
////
//// ========================================================================================
//// WifiP2PAcceptServerLink
////
//
//bool WifiP2PAcceptServerLink::doOpen(Anyfi::Address address) {
//    _sock = Anyfi::Socket::create(address, false);
//    protectFromVPN(_sock);
//
//    if (Anyfi::Socket::open(_sock, address)) {
//        _linkAddr.port(Anyfi::Socket::getSockAddress(_sock).port());
//        return true;
//    }
//    return false;
//}
//
//std::shared_ptr<Link> WifiP2PAcceptServerLink::doAccept() {
//    int newSock = Anyfi::Socket::accept(_sock);
//    if (newSock < 0) {
//        return nullptr;
//    }
//    return std::shared_ptr<WifiP2PHandshakeLink>(new WifiP2PHandshakeLink(newSock, _selector));
//}
//
//void WifiP2PAcceptServerLink::doClose() {
//    Anyfi::Socket::close(_sock);
//    _sock = -1;
//}

//
// WifiP2PExternalLink
// ========================================================================================
//
