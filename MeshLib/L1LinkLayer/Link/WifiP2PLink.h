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

#ifndef ANYFIMESH_CORE_WIFIP2PLINK_H
#define ANYFIMESH_CORE_WIFIP2PLINK_H

#include "Link.h"
#include "AcceptServerLink.h"
#include "../../L1LinkLayer/IOEngine/Selector.h"

#include <utility>

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif
//
//// Forward declarations
//class WifiP2PLink;
//class WifiP2PHandshakeLink;
//class WifiP2PInternalLink;
//class WifiP2PAcceptServerLink;
//
///**
// * A Wifi-P2P Link's handshake state.
// * A Wifi-P2P Link which doing handshake can be in one of the following states.
// */
//enum WifiP2pLinkHandshakeState {
//    /**
//     * A link that is not in handshake state.
//     */
//    NOT_IN_HANDSHAKE = 0,
//
//    /**
//     * A link that is waiting for handshake REQ packet is in this state.
//     */
//    WAITING_FOR_REQ = 1,
//
//    /**
//     * A link that has been sent handshake REQ packet is in this state.
//     */
//    REQ_SENT = 2,
//
//    /**
//     * A link that has been connected
//     */
//    LINK_CONNECTED = 3
//};
//
//
///**
// * A Link which supports Wifi-P2P protocol.
// *
// * This WifiP2PLink contains three of the links.
// * - Handshake Link : This link used for Wifi-P2P link handshake.
// * - Read Link : UDP link used to read from the peer. Read link is opened during the link-handshake process.
// * - Write Link : UDP link used to write to the peer. Write link is connected during the link-handshake process.
// */
//class WifiP2PLink : public Link {
//public:
//    /**
//     * Returns whether this link is owner in the Wifi-P2P group.
//     *
//     * @return true, if this link is owner in the Wifi-P2P group, false if this link is client.
//     */
//    bool isGroupOwner() const { return _isGroupOwner; }
//
//    /**
//     * Default constructor of the WifiP2PLink.
//     * Constructs with link id and link address.
//     *
//     * @param link_id id of the link
//     * @param link_addr address of the link
//     */
//    WifiP2PLink(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel)
//            : Link(std::move(selector), std::move(channel)), _isGroupOwner(false) {}
//    /**
//     * Constructor used when create the WifiP2PLink with a handshake link.
//     *
//     * @param link_id id of the link
//     * @param link_addr address of the link
//     * @param handshakeLink Wifi-P2P handshake link
//     */
//    WifiP2PLink(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel, std::shared_ptr<WifiP2PHandshakeLink> handshakeLink)
//            : Link(std::move(selector), std::move(channel)), _isGroupOwner(true), _handshakeLink(std::move(handshakeLink)) { }
//
//protected:
//    int validOps() const override {
//        return SelectionKeyOp::OP_ACCEPT |
//               SelectionKeyOp::OP_CONNECT |
//               SelectionKeyOp::OP_READ |
//               SelectionKeyOp::OP_WRITE;
//    };
//
//    void write(NetworkBufferPointer buffer); // override
//
//    Anyfi::SocketConnectResult doSockConnect(Anyfi::Address address) override;
//    bool finishSockConnect() override;
//
//    bool isHandshakeRequired() override { return true; }
//    LinkHandshakeResult doHandshake() override;
//    LinkHandshakeResult processHandshake(NetworkBufferPointer buffer) override;
//    bool checkHandshakeTimeout() override;
//
//    bool doOpen(Anyfi::Address address) override;
//    std::shared_ptr<Link> doAccept() override;
//
//    void doClose() override;
//    NetworkBufferPointer doRead() override;
//    void doWrite(NetworkBufferPointer buffer) override;
//
//protected:
//    friend class WifiP2PInternalLink;
//    friend class WifiP2PHandshakeLink;
//    friend class L1LinkLayer;
//    Anyfi::Address openReadLink();
//    void connectWriteLink(const Anyfi::Address &writeAddr);
//
//private:
//    const bool _isGroupOwner;
//    std::shared_ptr<WifiP2PHandshakeLink> _handshakeLink;
//    std::shared_ptr<WifiP2PInternalLink> _readUDPLink, _writeUDPLink;
//};
//
//
///**
// * This link
// */
//class WifiP2PInternalLink : public Link {
//protected:
//    friend class WifiP2PLink;
//    /**
//     * Default constructor of the WifiP2PInternalLink.
//     * Constructs with parent WifiP2PLink.
//     *
//     * @param parent Parent WifiP2PLink.
//     */
//    explicit WifiP2PInternalLink(const std::shared_ptr<WifiP2PLink> &parent)
//            : Link(parent->selector()), _parent(parent) {}
//    /**
//     * Constructor used when create a handshake link with socket (Accept from WifiP2PAcceptLink)
//     *
//     * @param sock new accepted socket number
//     */
//    WifiP2PInternalLink(const int sock, std::shared_ptr<Selector> selector)
//            : Link(std::move(selector)) {
//        _sock = sock;
//    }
//
//    int validOps() const override {
//        return SelectionKeyOp::OP_CONNECT | SelectionKeyOp::OP_READ | SelectionKeyOp::OP_WRITE;
//    };
//
//    Anyfi::SocketConnectResult doSockConnect(Anyfi::Address address) override;
//    bool finishSockConnect() override;
//
//    bool isHandshakeRequired() override { return false; }
//    LinkHandshakeResult doHandshake() override { throw std::runtime_error("Unsupported Method"); }
//    LinkHandshakeResult processHandshake(NetworkBufferPointer buffer) override { throw std::runtime_error("Unsupported Method"); }
//    bool checkHandshakeTimeout() override { throw std::runtime_error("Unsupported Method"); }
//
//    bool doOpen(Anyfi::Address address) override;
//    std::shared_ptr<Link> doAccept() override;
//
//    void doClose() override;
//    NetworkBufferPointer doRead() override;
//    void doWrite(NetworkBufferPointer buffer) override;
//
//protected:
//    std::shared_ptr<WifiP2PLink> _parent;
//};
//
//
///**
// * A handshake link for {@link WifiP2PLink}.
// *
// * This link used for WifiP2P-link handshake.
// * All packets from this link travel through the TCP connection.
// *
// * The handshake process is shown below.
// * > Connect handshake process
// * >  - First of all, open parent's read link.
// * >  - Secondly, send a REQ handshake packet with opened read link information.
// * >  - Lastly, receive a RES handshake packet which contains opponent's read link information,
// *      and connect parent's write link with that information.
// * > Accept handshake process
// * >  - First of all, receive a REQ handshake packet which contains opponent's read link information,
// *      connect parent's write link with that information.
// * >  - Secondly, open parent's read link.
// * >  - Lastly, send a REQ handshake packet with opened read link information.
// */
//class WifiP2PHandshakeLink : public HandshakeLink {
//protected:
//    friend class WifiP2PLink;
//    friend class WifiP2PAcceptServerLink;
//
//    WifiP2PHandshakeLink(const std::shared_ptr<WifiP2PLink> &parent)
//            : HandshakeLink(parent) {
//        state(LinkState::LINK_HANDSHAKING);
//    }
//    WifiP2PHandshakeLink(const int sock, std::shared_ptr<Selector> selector)
//            : HandshakeLink(sock, std::move(selector)) {
//        state(LinkState::LINK_HANDSHAKING);
//    }
//
//    int validOps() const override {
//        return SelectionKeyOp::OP_CONNECT |
//               SelectionKeyOp::OP_READ |
//               SelectionKeyOp::OP_WRITE;
//    };
//
//    Anyfi::SocketConnectResult doSockConnect(Anyfi::Address address) override;
//    bool finishSockConnect() override;
//
//    bool isHandshakeRequired() override { return true; }
//    LinkHandshakeResult doHandshake() override;
//    LinkHandshakeResult processHandshake(NetworkBufferPointer buffer) override;
//    bool checkHandshakeTimeout() override;
//
//    bool doOpen(Anyfi::Address address) override;
//    std::shared_ptr<Link> doAccept() override;
//
//    void doClose() override;
//    NetworkBufferPointer doRead() override;
//    void doWrite(NetworkBufferPointer buffer) override;
//
//
//private:
//    void sendHandshakeREQ(Anyfi::Address address);
//    void sendHandshakeRES(Anyfi::Address address);
//    void processHandshakeREQ(NetworkBufferPointer buffer);
//    void processHandshakeRES(NetworkBufferPointer buffer);
//};
//
//
///**
// * An acceptor which accepts WifiP2PHandshakeLink's connection.
// * This acceptor returns WifiP2PHandshakeLink as a result of the accept.
// */
//class WifiP2PAcceptServerLink : public AcceptServerLink {
//public:
//    WifiP2PAcceptServerLink(unsigned short link_id, std::shared_ptr<Selector> selector)
//            : AcceptServerLink(link_id, std::move(selector)) {}
//protected:
//
//    bool doOpen(Anyfi::Address address) override;
//    std::shared_ptr<Link> doAccept() override;
//    void doClose() override;
//
//
//#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
//    FRIEND_TEST(L1LinkLayer, AcceptWifiP2PLink);
//#endif
//};


#endif //ANYFIMESH_CORE_WIFIP2PLINK_H
