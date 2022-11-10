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

#include "Channel.h"
#include "ProxyChannel.h"


void BaseChannel::onRead(const std::shared_ptr<Link> &link, const NetworkBufferPointer &buffer) {
    if (_readCallback)
        _readCallback(shared_from_this(), link, buffer);

    if (buffer != nullptr) {
        if (!link->addr().addr().empty() && link->addr().connectionType() == Anyfi::ConnectionType::Proxy) {
            auto proxyChannel = std::dynamic_pointer_cast<ProxyChannel>(shared_from_this());

            // Data read. Ack must have been received
            proxyChannel->clearLastWriteTime();

            // Track TCP performance
            if (link->addr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
                proxyChannel->updateTCPInfo();
            }
        }
    }
}

void BaseChannel::onAccepted(const std::shared_ptr<Link> &newLink) {
    if (_acceptCallback)
        _acceptCallback(shared_from_this(), newLink);
}

bool BaseChannel::operator==(const BaseChannel &other) const {
    return (this->_id == other._id);
}

bool BaseChannel::operator<(const BaseChannel &other) const {
    return (this->_id < other._id);
};

std::vector<int> SimpleChannel::socks() {
    std::vector<int> socks = {_link->sock()};
    return socks;
}

std::vector<int> ComplexChannel::socks() {
    std::vector<int> socks;
    for (const auto &link : _links) {
        if (link)
            socks.push_back(link->sock());
    }
    return socks;
}

//
const std::string BaseChannel::toString()
{
    std::string dumped = "BaseChannel " + to_string(this)
                         + " state=" + to_string((int)state())
                         + " addr=" + addr().addr()
    ;
    return dumped;
}

const std::string SimpleChannel::toString()
{
    std::string dumped = "SimpleChannel " + to_string(this)
                         + " state=" + to_string((int)state())
                         + " addr=" + addr().addr()
                         + " link=" + to_string(_link.get())
    ;
    return dumped;
}


const std::string ComplexChannel::toString()
{
    std::string dumped = "ComplexChannel " + to_string(this)
                         + " state=" + to_string((int)state())
                         + " addr=" + addr().addr()
    ;
    for(auto it = _links.begin(); it != _links.end(); it++) {
        dumped += " link=" + to_string(it->get());
    }
    return dumped;
}
