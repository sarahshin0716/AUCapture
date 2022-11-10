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

#include "P2PLinkHandshakePacket.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"


P2PLinkHandshakePacket::P2PLinkHandshakePacket(P2PLinkHandshakePacket::Type type, const std::vector<Anyfi::Address>& addresses) {
    _type = type;
    for(auto it = addresses.begin(); it != addresses.end(); it++) {
        link_addr addr;
        addr.af = it->type() == Anyfi::AddrType::IPv4 ? IPv4 : IPv6;
        addr.ipAddr = it->addr();
        addr.port = it->port();
        _addrs.push_back(addr);
    }
}

P2PLinkHandshakePacket::P2PLinkHandshakePacket(NetworkBufferPointer buffer) {
    _type = static_cast<P2PLinkHandshakePacket::Type>(buffer->get<uint8_t>());
    uint8_t addrCount = buffer->get<uint8_t>();
    for(uint8_t i = 0; i < addrCount; i++) {
        link_addr addr;
        addr.af = static_cast<P2PLinkHandshakePacket::AF>(buffer->get<uint8_t>());
        auto iplen = buffer->get<uint16_t>();
        uint8_t iptmp[INET6_ADDRSTRLEN + 1];
        buffer->getBytes(iptmp, iplen); iptmp[iplen] = 0;
        addr.ipAddr = std::string((const char *) iptmp);
        addr.port = buffer->get<uint16_t>();
        _addrs.push_back(addr);
    }
}

const std::vector<Anyfi::Address> P2PLinkHandshakePacket::addrs() const {
    std::vector<Anyfi::Address> addrs;

    for(size_t i = 0; i < _addrs.size(); i++) {
        auto addr = Anyfi::Address();
        addr.type(_addrs[i].af == IPv4 ? Anyfi::AddrType::IPv4 : Anyfi::AddrType::IPv6);
        addr.addr(_addrs[i].ipAddr);
        addr.port(_addrs[i].port);
        addrs.push_back(addr);
    }
    return addrs;
}

NetworkBufferPointer P2PLinkHandshakePacket::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();

    for(size_t i = 0; i < _addrs.size(); i++) {
        auto addr = _addrs[i];

        buffer->put<uint16_t>(addr.port);
        buffer->putBytes(addr.ipAddr.c_str());
        buffer->put<uint16_t>((uint16_t) addr.ipAddr.size());
        buffer->put<uint8_t>((uint8_t)addr.af);
    }
    buffer->put<uint8_t>((uint8_t)_addrs.size());
    buffer->put<uint8_t>((uint8_t)_type);
    return buffer;
}

P2PLinkHandshakePacket::Type P2PLinkHandshakePacket::getType(NetworkBufferPointer buffer) {
    return static_cast<P2PLinkHandshakePacket::Type>(buffer->get<uint8_t>(buffer->getReadPos()));
}
