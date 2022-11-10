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

#ifndef ANYFIMESH_CORE_TCPIPL4_H
#define ANYFIMESH_CORE_TCPIPL4_H

#include "../../../L3TransportLayer/Packet/Packet.h"
#include "../../../L3TransportLayer/Packet/PacketHeader.h"


namespace TCPIP {

/**
 * The packet header of fourth-lowest layer of the TCP/IP Reference Model.
 */
class L4Header : public PacketHeader {
public:
    L4Header(const NetworkBufferPointer &buffer, unsigned short offset = 0)
            : PacketHeader(buffer, offset) {}
};

/**
 * The packet of four-th lowest layer of the TCP/IP Referenece Model.
 */
template<class H>
class L4Packet : public Packet<H> {
public:
    L4Packet(const NetworkBufferPointer &buffer, unsigned short offset = 0)
            : Packet<H>(buffer, offset) {}

    unsigned short size() const override = 0;
};

}   // TCPIP namespace


#endif //ANYFIMESH_CORE_TCPIPL4_H
