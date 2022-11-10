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

#include "WifiP2PAcceptChannel.h"
#include "../../L1LinkLayer/Link/SocketLink.h"


void WifiP2PAcceptChannel::connect(Anyfi::Address address, const std::function<void(bool result)> &callback) {
    throw std::runtime_error("Unsupported Method");
}

bool WifiP2PAcceptChannel::open(Anyfi::Address address) {
    _address = address;

    _link = std::make_shared<SocketLink>(_selector, shared_from_this());
    auto result = _link->open(address);
    if (result) {
        auto assignedAddr = Anyfi::Socket::getSockAddress(_link->sock());
        _address.port(assignedAddr.port());
        return true;
    }
    return false;
}

void WifiP2PAcceptChannel::close() {
    _link->closeWithoutNotify();
    _link = nullptr;
}
void WifiP2PAcceptChannel::onClose(std::shared_ptr<Link> link) {
    _link = nullptr;
    if (_closeCallback)
        _closeCallback(shared_from_this());
}
void WifiP2PAcceptChannel::write(NetworkBufferPointer buffer) {
    throw std::runtime_error("Unsupported Method");
}


