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

#ifndef ANYFIMESH_CORE_ANYFIHEADER_H
#define ANYFIMESH_CORE_ANYFIHEADER_H

#include "TCPIPL4.h"


/**
 * Type of the protocols in Transport Layer.
 */
enum class MeshProtocol : uint8_t {
    ICMP = 1,
    RUDP = 6,
    UDP = 17,
};

/**
 * Base class of the Anyfi packet header.
 *
 * Fields in MeshProtocolHeader
 *  Bits || 0 - - - | 4 - - - || 8 - - - | 12- - - || 16- - - | 20- - - || 24- - - | 28- - - ||
 * Bytes || 0 - - - | - - - - || 1 - - - | - - - - || 2 - - - | - - - - || 3 - - - | 4 - - - ||
 * ===== || ================================================================================ ||
 *     0 ||   Mesh Protocol   ||                 <Depends on Mesh Protocol>                  ||
 *     4 ||       Anyfi Transport Source Port      ||    Anyfi Transport Destination Port    ||
 *     8 ||                            <Depends on Mesh Protocol>                            ||
 *    12 ||                            <Depends on Mesh Protocol>                            ||
 * bytes || ================================================================================ ||
 *
 * There are three fixed fields.
 * - Mesh Protocol : The first header field in the MeshProtocol header is the 8-bit Anyfi Transport Protocol field.
 *                  It indicates a type of Anyfi Transport Protocol.
 * - Anyfi Transport Source Port : Source virtual port in Anyfi Transport Layer.
 * - Anyfi Transport Destination Port : Destination virtual port in Anyfi Transport Layer.
 * - Other fields are depending on Anyfi Transport Protocol.
 */
class MeshProtocolHeader : public TCPIP::L4Header {
public:
    MeshProtocolHeader(const NetworkBufferPointer &buffer)
            : TCPIP::L4Header(buffer) {}
    MeshProtocolHeader(const NetworkBufferPointer &buffer, unsigned short offset)
            : TCPIP::L4Header(buffer, offset) {}

    /**
     * Returns a length of the packet header.
     * It depends on the Anyfi Transport Protocol.
     */
    unsigned short length() const override { return 0; }

    MeshProtocol protocol() const;
    void protocol(MeshProtocol protocol);

    uint16_t sourcePort() const;
    void sourcePort(uint16_t sourcePort);

    uint16_t destPort() const;
    void destPort(uint16_t destPort);
};


#endif //ANYFIMESH_CORE_ANYFIHEADER_H
