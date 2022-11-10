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

#include "UDPHeader.h"

#include "../../../L3TransportLayer/Packet/TCPIP_L2_Internet/IPv4Header.h"


#define IPPROTO_UDP 17


uint16_t udp_checksum(const void *buff, size_t len, uint32_t src_addr, uint32_t dest_addr) {
    const uint16_t *buf = (uint16_t *) buff;
    uint16_t *ip_src = (uint16_t *) &src_addr, *ip_dst = (uint16_t *) &dest_addr;
    uint32_t sum;
    size_t length = len;

    // Calculate the sum
    sum = 0;
    while (len > 1) {
        sum += *buf++;
        if (sum & 0x80000000)
            sum = (sum & 0xFFFF) + (sum >> 16);
        len -= 2;
    }

    if (len & 1)
        // Add the padding if the packet length is odd
        sum += *((uint8_t *) buf);

    // Add the pseudo-header
    sum += *(ip_src++);
    sum += *ip_src;

    sum += *(ip_dst++);
    sum += *ip_dst;

    sum += Endian::hostToNetworkOrder<uint16_t>(IPPROTO_UDP);
    sum += Endian::hostToNetworkOrder<uint16_t>((uint16_t) length);

    // Add the carries
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    // Return the one's complement of sum
    return ((uint16_t) (~sum));
}

void UDPHeader::updateChecksum(IPHeader *ipHeader) {
    if (ipHeader->version() == IPVersion::Version4) {
        IPv4Header *ipv4Header = dynamic_cast<IPv4Header *>(ipHeader);

        checksum(0);

        uint8_t *p;
        if (_buffer->isBackwardMode())
            p = _buffer->buffer() + _buffer->size() - _startPosition - length();
        else p = _buffer->buffer() + _startPosition;

		checksum(Endian::hostToNetworkOrder<uint16_t>(udp_checksum(p, ipv4Header->totalLength() - ipv4Header->ihl(),
			Endian::hostToNetworkOrder<uint32_t>(ipv4Header->sourceAddress()),
			Endian::hostToNetworkOrder<uint32_t>(ipv4Header->destAddress()))));
    } else {
        // Not yet supported
    }
}

void UDPHeader::updateIPv4Checksum(uint32_t srcAddr, uint32_t dstAddr, uint32_t udpLength) {
    checksum(0);

    uint8_t *p;
    if (_buffer->isBackwardMode())
        p = _buffer->buffer() + _buffer->size() - _startPosition - UDP_HEADER_LENGTH;
    else p = _buffer->buffer() + _startPosition;
    checksum(Endian::hostToNetworkOrder<uint16_t>(udp_checksum(p, udpLength,
		Endian::hostToNetworkOrder<uint32_t>(srcAddr), 
		Endian::hostToNetworkOrder<uint32_t>(dstAddr))));
}

uint16_t UDPHeader::sourcePort() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(0));
}

void UDPHeader::sourcePort(uint16_t sourcePort) {
    _buffer->put<uint16_t>(sourcePort, _comptPos<uint16_t>(0));
}

uint16_t UDPHeader::destPort() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(2));
}

void UDPHeader::destPort(uint16_t destPort) {
    _buffer->put<uint16_t>(destPort, _comptPos<uint16_t>(2));
}

uint16_t UDPHeader::udpLength() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(4));
}

void UDPHeader::udpLength(uint16_t udpLength) {
    _buffer->put<uint16_t>(udpLength, _comptPos<uint16_t>(4));
}

uint16_t UDPHeader::checksum() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(6));
}

void UDPHeader::checksum(uint16_t checksum) {
    _buffer->put<uint16_t>(checksum, _comptPos<uint16_t>(6));
}
//
// UDP header fields
// ========================================================================================
//


//
// ========================================================================================
// UDP header builder
//
void UDPHeaderBuilder::sourcePort(uint16_t srcPort) {
    _sourcePort = srcPort;
}

void UDPHeaderBuilder::destPort(uint16_t dstPort) {
    _destPort = dstPort;
}

void UDPHeaderBuilder::udpLength(uint16_t udpLen) {
    _udpLength = udpLen;
}

void UDPHeaderBuilder::checksum(uint16_t sum) {
    _checksum = sum;
}

void UDPHeaderBuilder::srcAddr(uint32_t addr) {
    _srcAddr = addr;
}

void UDPHeaderBuilder::dstAddr(uint32_t addr) {
    _dstAddr = addr;
}

void UDPHeaderBuilder::setSrcAddrPort(Anyfi::Address &addr) {
    _srcAddr = addr.getIPv4AddrAsNum();
    _sourcePort = addr.port();
}

void UDPHeaderBuilder::setSrcAddrPort(Anyfi::Address &&addr) {
    _srcAddr = addr.getIPv4AddrAsNum();
    _sourcePort = addr.port();
}

void UDPHeaderBuilder::setDstAddrPort(Anyfi::Address &addr) {
    _dstAddr = addr.getIPv4AddrAsNum();
    _destPort = addr.port();
}

void UDPHeaderBuilder::setDstAddrPort(Anyfi::Address &&addr) {
    _dstAddr = addr.getIPv4AddrAsNum();
    _destPort = addr.port();
}

std::shared_ptr<UDPHeader> UDPHeaderBuilder::build(NetworkBufferPointer buffer, unsigned short offset) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    if (buffer->size() < offset + UDP_HEADER_LENGTH) {
        auto writePos = buffer->getWritePos();
        auto readPos = buffer->getReadPos();
        buffer->resize(offset + UDP_HEADER_LENGTH);
        buffer->setWritePos(writePos);
        buffer->setReadPos(readPos);
    }

    auto udpHeader = std::make_shared<UDPHeader>(buffer, offset);
    udpHeader->sourcePort(_sourcePort);
    udpHeader->destPort(_destPort);
    udpHeader->udpLength(_udpLength);
    udpHeader->checksum(_checksum);

    if (_checksum == 0) {
        if (_srcAddr == 0 || _dstAddr == 0)
            throw std::invalid_argument("UDP Checksum pseudo info required");
        udpHeader->updateIPv4Checksum(_srcAddr, _dstAddr, _udpLength);
    }

    if (buffer->getWritePos() < offset + UDP_HEADER_LENGTH)
        buffer->setWritePos(offset + UDP_HEADER_LENGTH);

    return udpHeader;
}
//
// UDP header builder
// ========================================================================================
//
