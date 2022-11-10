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

#ifndef ANYFIMESH_CORE_IPHEADER_H
#define ANYFIMESH_CORE_IPHEADER_H

#include <cstdint>

#include "../../../L3TransportLayer/Packet/PacketHeader.h"
#include "TCPIPL2.h"


enum class IPVersion : uint8_t {
    Version4 = 4,
    Version6 = 6,
    Undefined = 0,
};

enum class IPNextProtocol : uint8_t {
    HOPBYHOP = 0,
    ICMP = 1,
    TCP = 6,
    UDP = 17,
    ICMPv6 = 58
};

/**
 * Base class of the IP packet header.
 */
class IPHeader : public TCPIP::L2Header {
public:
    IPHeader(const NetworkBufferPointer &buffer)
            : TCPIP::L2Header(buffer) {}
    IPHeader(const NetworkBufferPointer &buffer, unsigned short offset)
            : TCPIP::L2Header(buffer, offset) {}

    /**
     * Returns a length of the packet header.
     */
    unsigned short length() const override = 0;

    IPVersion version() const;
    void version(IPVersion version);


    /**
     * Get IP Version from the given buffer.
     */
    static IPVersion getVersionFromBuffer(const NetworkBufferPointer &buffer, unsigned short startPosition);
    /**
     * Get IP Version from the given byte.
     */
    static IPVersion getVersionFromByte(uint8_t &byte);
};


#endif //ANYFIMESH_CORE_IPHEADER_H
