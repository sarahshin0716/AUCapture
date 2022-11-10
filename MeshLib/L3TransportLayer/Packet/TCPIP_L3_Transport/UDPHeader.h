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

#ifndef ANYFIMESH_CORE_UDPHEADER_H
#define ANYFIMESH_CORE_UDPHEADER_H

#include "../../../L3TransportLayer/Packet/TCPIP_L2_Internet/IPHeader.h"
#include "../../../Common/Network/Address.h"
#include "TCPIPL3.h"


#define UDP_HEADER_LENGTH 8


class UDPHeader : public TCPIP::L3Header {
public:
    UDPHeader(const NetworkBufferPointer &buffer) : TCPIP::L3Header(buffer) {}
    UDPHeader(const NetworkBufferPointer &buffer, unsigned short startPosition)
            : TCPIP::L3Header(buffer, startPosition) {}

    /**
     * Returns a length of the packet header.
     */
    unsigned short length() const override { return UDP_HEADER_LENGTH; }

    /**
     * Calculate a TCPIP L3 header checksum, and update the checksum field.
     */
    void updateChecksum(IPHeader *ipHeader) override;
    void updateIPv4Checksum(uint32_t srcAddr, uint32_t dstAddr, uint32_t udpLength) override;

//
// ========================================================================================
// UDP header fields
//
    uint16_t sourcePort() const;
    void sourcePort(uint16_t sourcePort);

    uint16_t destPort() const;
    void destPort(uint16_t destPort);

    uint16_t udpLength() const;
    void udpLength(uint16_t udpLength);

    uint16_t checksum() const;
    void checksum(uint16_t checksum);
//
// UDP header fields
// ========================================================================================
//
};


class UDPHeaderBuilder : public PacketHeaderBuilder<UDPHeader> {
public:
    /**
     * UDP header fields
     */
    void sourcePort(uint16_t srcPort);
    void destPort(uint16_t dstPort);
    void udpLength(uint16_t udpLen);
    void checksum(uint16_t sum);

    /**
     * Checksum pseudo header info
     */
    void srcAddr(uint32_t addr);
    void dstAddr(uint32_t addr);

    void setSrcAddrPort(Anyfi::Address &addr);
    void setSrcAddrPort(Anyfi::Address &&addr);
    void setDstAddrPort(Anyfi::Address &addr);
    void setDstAddrPort(Anyfi::Address &&addr);

    std::shared_ptr<UDPHeader> build(NetworkBufferPointer buffer, unsigned short offset = 0) override;

private:
    /**
     * UDP header fields
     */
    uint16_t _sourcePort = 0;
    uint16_t _destPort = 0;
    uint16_t _udpLength = 0;
    uint16_t _checksum = 0;

    /**
     * Checksum pseudo header info
     */
    uint32_t _srcAddr = 0;
    uint32_t _dstAddr = 0;
};


#endif //ANYFIMESH_CORE_UDPHEADER_H
