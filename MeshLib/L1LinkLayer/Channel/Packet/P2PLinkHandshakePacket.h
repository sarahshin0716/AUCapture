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

#ifndef ANYFIMESH_CORE_P2PLINKHANDSHAKEPACKET_H
#define ANYFIMESH_CORE_P2PLINKHANDSHAKEPACKET_H

#include <vector>

#include "../../../Common/Network/Address.h"
#include "../../../Common/Network/Buffer/NetworkBuffer.h"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 16
#endif


/**
 * 패킷 형태
 * |--------------------------------------------|
 * | 1 byte | INET6_ADDRSTRLEN bytes  | 2 byte  |
 * |--------------------------------------------|
 * | Type   | IPv6 Link-local-address | Port    |
 * |--------------------------------------------|
 */
class P2PLinkHandshakePacket {
public:
    enum Type : uint8_t {
        REQ = 1,
        RES = 2,
        LNK1 = 3,
        LNK2 = 4,
        LNK3 = 5,
    };
    enum AF : uint8_t {
        IPv4 = 1,
        IPv6 = 2
    };
    struct link_addr {
        AF af;
        std::string ipAddr;
        uint16_t port;
    };

    explicit P2PLinkHandshakePacket(Type type, const std::vector<Anyfi::Address>& addresses);
    explicit P2PLinkHandshakePacket(NetworkBufferPointer buffer);

    const std::vector<Anyfi::Address> addrs() const;
    Type type() const { return _type; }

    NetworkBufferPointer toPacket();

    static Type getType(NetworkBufferPointer buffer);

private:
    Type _type;
    std::vector<link_addr> _addrs;
};

#endif //ANYFIMESH_CORE_P2PLINKHANDSHAKEPACKET_H
