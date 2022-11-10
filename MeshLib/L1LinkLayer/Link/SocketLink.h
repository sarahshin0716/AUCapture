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

#ifndef ANYFIMESH_CORE_PROXYLINK_H
#define ANYFIMESH_CORE_PROXYLINK_H

#include <utility>
#include <linux/tcp.h>

#include "Link.h"
#include "../../L1LinkLayer/IOEngine/Selector.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif


/**
 * A link that communicates with Proxy Network(Outside of the mesh network).
 */
class SocketLink : public Link {
public:
    SocketLink(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel)
            : Link(std::move(selector), std::move(channel)) {}
    SocketLink(int sock, std::shared_ptr<Selector> selector)
            : Link(std::move(selector), nullptr) {
        _sock = sock;
    }

    void setReadLimit(size_t limit) { _readLimit = limit; }
protected:
    int validOps() const override {
        return SelectionKeyOp::OP_CONNECT |
               SelectionKeyOp::OP_READ |
               SelectionKeyOp::OP_WRITE |
                SelectionKeyOp::OP_ACCEPT;
    };

    Anyfi::SocketConnectResult doSockConnect(Anyfi::Address address, const std::string &ifname) override;
    bool finishSockConnect() override;

    bool isHandshakeRequired() override { return false; }
    LinkHandshakeResult doHandshake() override {
        throw std::runtime_error("Unsupported Method"); }
    LinkHandshakeResult processHandshake(NetworkBufferPointer buffer) override {
        throw std::runtime_error("Unsupported Method"); }
    bool checkHandshakeTimeout() override {
        throw std::runtime_error("Unsupported Method"); }

    bool doOpen(Anyfi::Address address, const std::string& in = "") override;
    std::shared_ptr<Link> doAccept() override;

    void doClose() override;
    NetworkBufferPointer doRead() override;
    int doWrite(NetworkBufferPointer buffer) override;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(SocketLink, Connect);
    FRIEND_TEST(SocketLink, ConnectTimeout);
    FRIEND_TEST(SocketLink, CloseWhenConnected);
    FRIEND_TEST(ProxyChannel, Connect);
    FRIEND_TEST(ProxyChannel, ConnectTimeout);
    FRIEND_TEST(ProxyChannel, CloseWhenConnected);
#endif
};


#endif //ANYFIMESH_CORE_PROXYLINK_H
