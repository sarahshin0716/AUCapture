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

#ifndef ANYFIMESH_CORE_RUDPPACKET_H
#define ANYFIMESH_CORE_RUDPPACKET_H

#include "TCPIPL4.h"
#include "MeshRUDPHeader.h"


class MeshRUDPPacket : public TCPIP::L4Packet<MeshRUDPHeader> {
public:
    MeshRUDPPacket(const NetworkBufferPointer &buffer, unsigned short offset)
            : TCPIP::L4Packet<MeshRUDPHeader>(buffer, offset) {
        _header = std::make_shared<MeshRUDPHeader>(buffer, offset);
    }

    /**
     * Returns a size of the packet.
     */
    unsigned short size() const override;

    std::shared_ptr<MeshRUDPHeader> header() const override;
};


#endif //ANYFIMESH_CORE_RUDPPACKET_H
