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

#ifndef ANYFIMESH_CORE_ANYFIUDPPACKET_H
#define ANYFIMESH_CORE_ANYFIUDPPACKET_H

#include "TCPIPL4.h"
#include "MeshUDPHeader.h"


class MeshUDPPacket : public TCPIP::L4Packet<MeshUDPHeader> {
public:
    MeshUDPPacket(const NetworkBufferPointer &buffer, unsigned short offset)
            : TCPIP::L4Packet<MeshUDPHeader>(buffer, offset) {
        _header = std::make_shared<MeshUDPHeader>(buffer, offset);
    }
};


#endif //ANYFIMESH_CORE_ANYFIUDPPACKET_H
