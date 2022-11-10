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

#ifndef ANYFIMESH_CORE_MESHENETRUDPHEADER_H
#define ANYFIMESH_CORE_MESHENETRUDPHEADER_H

#include "MeshProtocolHeader.h"


#define MESH_ENET_RUDP_HEADER_LENGTH 8


/**
 * Mesh Enet RUDP pakcet header
 */
class MeshEnetRUDPHeader : public MeshProtocolHeader {
public:
    enum Type : uint8_t {
        DATAGRAM        = 0,
        CLOSE_REQ       = 0b00000001,
        CLOSE_RES       = 0b00000010,
        CLOSE_REQ_RES   = CLOSE_REQ | CLOSE_RES,
    };

    MeshEnetRUDPHeader(const NetworkBufferPointer &buffer)
            : MeshProtocolHeader(buffer) {}
    MeshEnetRUDPHeader(const NetworkBufferPointer &buffer, unsigned short offset)
            : MeshProtocolHeader(buffer, offset) {}

    /**
     * Returns a header length
     */
    unsigned short length() const override { return MESH_ENET_RUDP_HEADER_LENGTH; }

    uint8_t type() const;
    void type(uint8_t type);

    uint16_t reserved() const;
    void reserved(uint16_t reserved);
};


/**
 * MeshEnetRUDP packet header builder
 */
class MeshEnetRUDPHeaderBuilder : public PacketHeaderBuilder<MeshEnetRUDPHeader> {
public:
    void type(uint8_t type);
    void sourcePort(uint16_t sourcePort);
    void destPort(uint16_t destPort);

    std::shared_ptr<MeshEnetRUDPHeader> build(NetworkBufferPointer buffer, unsigned short offset = 0) override;

private:
    uint8_t _type = MeshEnetRUDPHeader::Type::DATAGRAM;
    uint16_t _sourcePort = 0;
    uint16_t _destPort = 0;
};


#endif //ANYFIMESH_CORE_MESHENETRUDPHEADER_H
