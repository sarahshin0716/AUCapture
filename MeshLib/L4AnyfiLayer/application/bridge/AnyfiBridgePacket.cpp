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

#include "AnyfiBridgePacket.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"

AnyfiBridgePacket::AnyfiBridgePacket() {
    _type = AnyfiBridgePacket::Type::Unknown;
}

AnyfiBridgePacket::Type AnyfiBridgePacket::getType(NetworkBufferPointer buffer) {
    return static_cast<AnyfiBridgePacket::Type>(buffer->get<uint8_t>(buffer->getReadPos()));
}

/**
 * AnyfiProxyConnPacket
 **/
AnyfiBridgeConnPacket::AnyfiBridgeConnPacket() {
    _type = AnyfiBridgePacket::Type::Connect;
    _destAddr = Anyfi::Address();
}
AnyfiBridgeConnPacket::AnyfiBridgeConnPacket(const Anyfi::Address &proxyAddr) {
    _type = AnyfiBridgePacket::Type::Connect;
    _destAddr = proxyAddr;
}
NetworkBufferPointer AnyfiBridgeConnPacket::toPacket() {
    auto buffer = NetworkBufferPool::getInstance()->acquire();
    buffer->setBackwardMode();
    buffer->put<uint8_t>(static_cast<uint8_t>(_destAddr.connectionType()));
    buffer->put<uint8_t>(static_cast<uint8_t>(_destAddr.connectionProtocol()));
    buffer->put<uint16_t>(_destAddr.port());
    buffer->put<uint32_t>(_destAddr.getIPv4AddrAsNum());
    buffer->put<uint8_t>((uint8_t)_type);
    return buffer;
}
std::string AnyfiBridgeConnPacket::toString() {
    return "{ type: "+AnyfiBridgePacket::toString(_type) +
            ", ip: "+ _destAddr.getIPv4Addr() +
            ", port:"+ to_string(_destAddr.port()) +
            ", protocol: "+ to_string((int)_destAddr.connectionProtocol()) +
            " }";
}
std::shared_ptr<AnyfiBridgeConnPacket> AnyfiBridgeConnPacket::toSerialize(NetworkBufferPointer buffer) {
    auto type = static_cast<AnyfiBridgePacket::Type>(buffer->get<uint8_t>());
    if (type != AnyfiBridgePacket::Type::Connect) {
        std::cerr << "Type not match Type::Connect. (value: " << type << ")";
        return nullptr;
    }else {
        auto ip = buffer->get<uint32_t>();
        auto port = buffer->get<uint16_t>();
        auto protocol = static_cast<Anyfi::ConnectionProtocol>(buffer->get<uint8_t>());
        auto connectionType = static_cast<Anyfi::ConnectionType>(buffer->get<uint8_t>());
        Anyfi::Address addr = Anyfi::Address();
        addr.setIPv4Addr(ip);
        addr.port(port);
        addr.connectionProtocol(protocol);
        addr.connectionType(connectionType);
        return std::make_shared<AnyfiBridgeConnPacket>(addr);
    }
}

/**
 * AnyfiProxyDataPacket
 **/
AnyfiBridgeDataPacket::AnyfiBridgeDataPacket() {
    _type = AnyfiBridgePacket::Type::Data;
    _payload = nullptr;
}
AnyfiBridgeDataPacket::AnyfiBridgeDataPacket(const NetworkBufferPointer &buffer) {
    _type = AnyfiBridgePacket::Type::Data;
    _payload = buffer;

}
NetworkBufferPointer AnyfiBridgeDataPacket::toPacket() {
    auto buffer = NetworkBufferPool::getInstance()->acquire();
    buffer->setBackwardMode();
    buffer->putBytes(_payload.get());
    buffer->put<uint8_t>((uint8_t)_type);
    return buffer;
}

std::string AnyfiBridgeDataPacket::toString() {
    return "{ type: "+AnyfiBridgePacket::toString(_type) +
           ", payload: "+ ((_payload!=nullptr)?to_string(_payload->size()):"nullptr" )+
           " }";
}
std::shared_ptr<AnyfiBridgeDataPacket> AnyfiBridgeDataPacket::toSerialize(NetworkBufferPointer buffer) {
    auto type = static_cast<AnyfiBridgePacket::Type>(buffer->get<uint8_t>());
    if (type != AnyfiBridgePacket::Type::Data) {
        std::cerr << "Type not match Type::Connect. (value: " << type << ")";
        return nullptr;
    }else {
        return std::make_shared<AnyfiBridgeDataPacket>(buffer);
    }
}