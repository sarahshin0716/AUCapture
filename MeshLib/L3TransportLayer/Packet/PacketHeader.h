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

#ifndef ANYFIMESH_CORE_PACKETHEADER_H
#define ANYFIMESH_CORE_PACKETHEADER_H

#include <utility>

#include "../../Common/Network/Buffer/NetworkBuffer.h"


/**
 * Base class of packet header.
 */
class PacketHeader {
public:
    PacketHeader(NetworkBufferPointer buffer, unsigned short startPosition = 0)
            : _buffer(std::move(buffer)), _startPosition(startPosition) {}

    /**
     * Returns a length of the packet header.
     */
    virtual unsigned short length() const = 0;

    /**
     * Returns a buffer of the packet header.
     */
    NetworkBufferPointer buffer() const { return _buffer; }
    unsigned short startPosition() const { return _startPosition; }

protected:
    const NetworkBufferPointer _buffer;
    const unsigned short _startPosition;

    /**
     * Gets a compatible position which can use whether networkMode is forward or backward from the forward position.
     *
     * @tparam T The type of field data.
     * @param fieldPos The position in header fields.
     * @param headerLen The length of packet header.
     * @return backward position if networkMode is backward, forward position otherwise.
     */
    template<typename T>
    inline unsigned short _comptPos(unsigned short fieldPos, unsigned short headerLen) const {
        if (_buffer->isBackwardMode())
            return _startPosition + headerLen - fieldPos - sizeof(T);
        return _startPosition + fieldPos;
    }
    inline unsigned short _comptPos(unsigned short fieldPos, unsigned short size, unsigned short headerLen) const {
        if (_buffer->isBackwardMode())
            return _startPosition + headerLen - fieldPos - size;
        return _startPosition + fieldPos;
    }
    template<typename T>
    inline unsigned short _comptPos(unsigned short fieldPos) const {
        return _comptPos<T>(fieldPos, length());
    }
};


/**
 * Abstract Packet Header Builder
 *
 * @tparam H Type of packet header to build
 */
template<class H>
class PacketHeaderBuilder {
public:
    explicit PacketHeaderBuilder() {}
    virtual ~PacketHeaderBuilder() = default;

    virtual std::shared_ptr<H> build(NetworkBufferPointer buffer, unsigned short offset = 0) { return std::shared_ptr<H>(); }
};


#endif //ANYFIMESH_CORE_PACKETHEADER_H
