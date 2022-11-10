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

#ifndef ANYFIMESH_CORE_UDPPACKET_H
#define ANYFIMESH_CORE_UDPPACKET_H

#include "TCPIPL3.h"
#include "UDPHeader.h"


class UDPPacket : public TCPIP::L3Packet<UDPHeader> {
    UDPPacket(const NetworkBufferPointer &buffer, unsigned short offset = 0)
            : TCPIP::L3Packet<UDPHeader>(buffer, offset) {
        _header = std::make_shared<UDPHeader>(buffer, offset);
    }
};


#endif //ANYFIMESH_CORE_UDPPACKET_H
