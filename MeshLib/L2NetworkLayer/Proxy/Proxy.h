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

#ifndef ANYFIMESH_CORE_PROXYSTATE_H
#define ANYFIMESH_CORE_PROXYSTATE_H

#include <cstdint>
#include <string>

#include "../../L1LinkLayer/Link/Link.h"
#include "../../Common/Network/Address.h"

namespace L2 {

class Proxy {
public:
    Proxy() = default;
    Proxy(unsigned short channelId, const Anyfi::Address &dstAddr,
               LinkState linkState) {
        _channelId = channelId;
        _dstAddr = dstAddr;
        _linkState = linkState;
    }

    /**
     * Setters
     */
    void channelId(unsigned short channelId) { _channelId = channelId; }
    void dstAddr(const Anyfi::Address &dstAddr) { _dstAddr = dstAddr; }
    void linkState(LinkState linkState) { _linkState = linkState; }

    /**
     * Getters
     */
    unsigned short channelId() const { return _channelId; }
    Anyfi::Address dstAddr() const { return _dstAddr; }
    LinkState linkState() { return _linkState; }

    /**
     * Operator overridings
     */
    bool operator==(const Proxy &other) const;
    bool operator!=(const Proxy &other) const;

private:
    unsigned short _channelId;
    Anyfi::Address _dstAddr;
    LinkState _linkState;
};

} // namespace L2
#endif //ANYFIMESH_CORE_PROXYSTATE_H
