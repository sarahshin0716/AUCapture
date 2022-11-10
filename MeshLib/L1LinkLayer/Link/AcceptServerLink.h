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

#ifndef ANYFIMESH_CORE_ACCEPTSERVERLINK_H
#define ANYFIMESH_CORE_ACCEPTSERVERLINK_H

#include "../../L1LinkLayer/IOEngine/Selector.h"
#include "Link.h"


/**
 * An acceptor which accepts a P2P link's connection.
 * This acceptor only can be used at IOEngine.
 */
class AcceptServerLink : public Link {
protected:
    friend class IOEngine;
    AcceptServerLink(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel)
            : Link(std::move(selector), std::move(channel)) {}

    int validOps() const override {
        return SelectionKeyOp::OP_ACCEPT;
    };

    virtual bool doOpen(Anyfi::Address address, const std::string& in) override = 0;
    virtual std::shared_ptr<Link> doAccept() override = 0;
    virtual void doClose() override = 0;

// Do not override these methods.
private:
    Anyfi::SocketConnectResult doSockConnect(Anyfi::Address address, const std::string &ifname) override {
        throw std::runtime_error("Unsupported Method");
    }
    bool finishSockConnect() override {
        throw std::runtime_error("Unsupported Method");
    }

    bool isHandshakeRequired() override { return false; }
    LinkHandshakeResult doHandshake() override {
        throw std::runtime_error("Unsupported Method");
    }
    LinkHandshakeResult processHandshake(NetworkBufferPointer buffer) override {
        throw std::runtime_error("Unsupported Method");
    }
    bool checkHandshakeTimeout() override {
        throw std::runtime_error("Unsupported Method");
    }


    NetworkBufferPointer doRead() override {
        throw std::runtime_error("Unsupported Method");
    }
    int doWrite(NetworkBufferPointer buffer) override {
        throw std::runtime_error("Unsupported Method");
    }
};


#endif //ANYFIMESH_CORE_ACCEPTSERVERLINK_H
