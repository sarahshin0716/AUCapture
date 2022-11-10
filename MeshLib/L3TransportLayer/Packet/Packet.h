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

#ifndef ANYFIMESH_CORE_PACKET_H
#define ANYFIMESH_CORE_PACKET_H

#include <memory>

#include "../../Common/Network/Buffer/NetworkBuffer.h"
#include "PacketHeader.h"


/**
 * Base class of packet.
 */
template<class H>
class Packet {
public:
    Packet(NetworkBufferPointer buffer, unsigned short startPosition = 0)
            : _buffer(std::move(buffer)), _startPosition(startPosition) {}

    /**
     * Returns a size of the packet.
     */
    virtual unsigned short size() const = 0;

    virtual std::shared_ptr<H> header() const { return _header; }
    uint8_t *payload() const { return _buffer->buffer() + _startPosition + _header->length(); }
    unsigned short payloadPos() const { return _startPosition + _header->length(); }
    unsigned short payloadSize() const { return _buffer->size() - _startPosition - _header->length(); };

    unsigned short startPosition() const { return _startPosition; }
    NetworkBufferPointer buffer() const { return _buffer; };

protected:
    const NetworkBufferPointer _buffer;
    const unsigned short _startPosition;

    // Initialize this instance on constructor of the child packet class.
    std::shared_ptr<H> _header;
};


/**
 * Abstract Packet Builder
 *
 * @tparam P Type of packet to build
 */
template<class P, class H>
class PacketBuilder {
public:
    explicit PacketBuilder() {}
    virtual ~PacketBuilder() = default;

    void headerBuilder(PacketHeaderBuilder<H> &header);

    std::shared_ptr<P> build(NetworkBufferPointer buffer);

protected:
    std::shared_ptr<P> _packet;
    PacketHeaderBuilder<H> _headerBuilder;
};


#endif //ANYFIMESH_CORE_PACKET_H
