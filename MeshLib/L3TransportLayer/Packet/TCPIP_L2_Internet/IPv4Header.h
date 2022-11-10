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

#ifndef ANYFIMESH_CORE_IPV4HEADER_H
#define ANYFIMESH_CORE_IPV4HEADER_H

#include "IPHeader.h"


class IPv4Header : public IPHeader {
public:
    IPv4Header(const NetworkBufferPointer &buffer);
    IPv4Header(const NetworkBufferPointer &buffer, unsigned short offset, unsigned short length = 0);

    /**
     * Returns a length of the packet header.
     */
    unsigned short length() const override {
        if (_len != 0) return _len;
        return ihl();
    }

    /**
     * Calculate a IPv4 header checksum, and update the headerChecksum field.
     */
    void updateChecksum();

    /**
     * Gets an IHL from the given byte.
     */
    static unsigned short getIHLFromByte(uint8_t &byte);


//
// ========================================================================================
// IPv4 header fields
//
    uint8_t ihl() const;
    void ihl(uint8_t ihl);

    uint8_t typeOfService() const;
    void typeOfService(uint8_t typeOfService);

    uint16_t totalLength() const;
    void totalLength(uint16_t totalLength);

    uint16_t identification() const;
    void identification(uint16_t identification);

    uint8_t flags() const;
    void flags(uint8_t flags);

    uint16_t fragmentOffset() const;
    void fragmentOffset(uint16_t fragmentOffset);

    uint8_t ttl() const;
    void ttl(uint8_t ttl);

    IPNextProtocol protocol() const;
    void protocol(IPNextProtocol protocol);

    uint16_t headerChecksum() const;
    void headerChecksum(uint16_t headerChecksum);

    uint32_t sourceAddress() const;
    void sourceAddress(uint32_t sourceAddress);

    uint32_t destAddress() const;
    void destAddress(uint32_t destAddress);

    const uint8_t *ipOption() const;
    void ipOption(uint8_t *ipOption, uint8_t length);

    const uint8_t ipOptionLength() const;
    void ipOptionLength(uint8_t length);
//
// IPv4 header fields
// ========================================================================================
//

private:
    unsigned short _len;

};


class IPv4HeaderBuilder : public PacketHeaderBuilder<IPv4Header> {
public:
    void ihl(uint8_t v);
    void typeOfService(uint8_t v);
    void totalLength(uint16_t v);
    void identification(uint16_t v);
    void flags(uint8_t v);
    void fragmentOffset(uint16_t v);
    void ttl(uint8_t v);
    void protocol(IPNextProtocol v);
    void headerChecksum(uint16_t v);
    void sourceAddress(uint32_t v);
    void destAddress(uint32_t v);
    void ipOption(uint8_t *v, uint8_t len);

    std::shared_ptr<IPv4Header> build(NetworkBufferPointer buffer, unsigned short offset) override;

private:
    uint8_t _ihl = 20;
    uint8_t _typeOfService = 0;
    uint16_t _totalLength = 0;
    uint16_t _identification = 0;
    uint8_t _flags = 2;
    uint16_t _fragmentOffset = 0;
    uint8_t _ttl = 64;
    IPNextProtocol _protocol = IPNextProtocol::TCP;
    uint16_t _headerChecksum = 0;
    uint32_t _sourceAddress = 0;
    uint32_t _destAddress = 0;
    uint8_t *_ipOption = nullptr;
    uint8_t _ipOptionLength = 0;

    bool _isChecksumSet = false;
};


#endif //ANYFIMESH_CORE_IPV4HEADER_H
