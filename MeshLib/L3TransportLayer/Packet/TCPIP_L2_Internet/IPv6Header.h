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

#ifndef ANYFIMESH_CORE_IPV6HEADER_H
#define ANYFIMESH_CORE_IPV6HEADER_H

#include "IPHeader.h"


class IPv6Header : public IPHeader {
public:
    IPv6Header(const NetworkBufferPointer &buffer);
    IPv6Header(const NetworkBufferPointer &buffer, unsigned short offset);

    /**
     * Returns a length of the packet header.
     */
    unsigned short length() const override { return 40; }

//
// ========================================================================================
// IPv6 header fields
//
    uint16_t trafficClass() const;
    void trafficClass(uint16_t trafficClass);

    uint32_t flowLabel() const;
    void flowLabel(uint32_t flowLabel);

    uint16_t payloadLength() const;
    void payloadLength(uint16_t payloadLength);

    IPNextProtocol nextHeader() const;
    void nextHeader(IPNextProtocol payloadLength);

    uint8_t hopLimit() const;
    void hopLimit(uint8_t hopLimit);

    std::vector<uint32_t> sourceAddress() const;
    void sourceAddress(std::vector<uint32_t> sourceAddress);

    std::vector<uint32_t> destAddress() const;
    void destAddress(std::vector<uint32_t> destAddress);
};


#endif //ANYFIMESH_CORE_IPV6HEADER_H
