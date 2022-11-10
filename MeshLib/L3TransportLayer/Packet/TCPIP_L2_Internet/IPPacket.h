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

#ifndef ANYFIMESH_CORE_IPPACKET_H
#define ANYFIMESH_CORE_IPPACKET_H

#include "TCPIPL2.h"
#include "IPHeader.h"


class IPPacket : public TCPIP::L2Packet<IPHeader> {
public:
    IPPacket(const NetworkBufferPointer &buffer, unsigned short offset = 0)
            : TCPIP::L2Packet<IPHeader>(buffer, offset) {
        _initializeIPHeader();
    }

    /**
     * Returns a size of the packet.
     */
    unsigned short size() const override;

    std::shared_ptr<IPHeader> header() const override;

private:
    void _initializeIPHeader();
};


#endif //ANYFIMESH_CORE_IPPACKET_H
