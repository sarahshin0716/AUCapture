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

#ifndef ANYFIMESH_CORE_ANYFIUDPHEADER_H
#define ANYFIMESH_CORE_ANYFIUDPHEADER_H

#include "MeshProtocolHeader.h"


#define MESH_UDP_HEADER_LENGTH 8


/**
 * Fields in MeshUDPHeader
 *  Bits || 0 - - - | 4 - - - || 8 - - - | 12- - - || 16- - - | 20- - - || 24- - - | 28- - - ||
 * Bytes || 0 - - - | - - - - || 1 - - - | - - - - || 2 - - - | - - - - || 3 - - - | 4 - - - ||
 * ===== || ================================================================================ ||
 *     0 ||   Anyfi Protocol  ||                           Reserved                          ||
 *     4 ||       Anyfi Transport Source Port      ||    Anyfi Transport Destination Port    ||
 * bytes || ================================================================================ ||
 *
 * Unique fields in AnyfiUDPHeader
 * - Reserved : Not yet used. Set this field with 0.
 */
class MeshUDPHeader : public MeshProtocolHeader {
public:
    MeshUDPHeader(const NetworkBufferPointer &buffer)
            : MeshProtocolHeader(buffer) {}
    MeshUDPHeader(const NetworkBufferPointer &buffer, unsigned short offset)
            : MeshProtocolHeader(buffer, offset) {}

    /**
     * Returns a length of the packet header.
     */
    unsigned short length() const override;

    uint32_t reserved() const;
    void reserved(uint32_t reserved);
};


class MeshUDPHeaderBuilder : public PacketHeaderBuilder<MeshUDPHeader> {
public:
    void sourcePort(uint16_t sourcePort);
    void destPort(uint16_t destPort);

    std::shared_ptr<MeshUDPHeader> build(NetworkBufferPointer buffer, unsigned short offset = 0) override;

private:
    uint16_t _sourcePort = 0;
    uint16_t _destPort = 0;
};


#endif //ANYFIMESH_CORE_ANYFIUDPHEADER_H
