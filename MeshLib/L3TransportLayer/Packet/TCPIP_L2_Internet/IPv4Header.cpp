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

#if defined(ANDROID) || defined(__ANDROID__)
#include <sys/endian.h>
#endif

#include "IPv4Header.h"

IPv4Header::IPv4Header(const NetworkBufferPointer &buffer) : IPHeader(buffer) {}

IPv4Header::IPv4Header(const NetworkBufferPointer &buffer, unsigned short offset, unsigned short length)
        : IPHeader(buffer, offset), _len(length) {}

void IPv4Header::updateChecksum() {
    // Cast the data pointer to one that can be indexed.
    char *data;
    if (_buffer->isBackwardMode())
        data = (char *) (_buffer->buffer() + _buffer->size() - _startPosition - ihl());
    else data = (char *) _buffer->buffer() + _startPosition;

    size_t length = ihl();

    // Initialise the accumulator.
    uint64_t acc = 0xffff;

    // Handle any partial block at the start of the data.
    unsigned int offset = static_cast<unsigned int>(((uintptr_t) data) & 3);
    if (offset) {
        size_t count = 4 - offset;
        if (count > length) count = length;
        uint32_t word = 0;
        memcpy(offset + (char *) &word, data, count);
        acc += Endian::networkToHostOrder<uint32_t>(word);
        data += count;
        length -= count;
    }

    // Handle any complete 32-bit blocks.
    char *data_end = data + (length & ~3);
    while (data != data_end) {
        uint32_t word;
        memcpy(&word, data, 4);
        acc += Endian::networkToHostOrder<uint32_t>(word);
        data += 4;
    }
    length &= 3;

    // Handle any partial block at the end of the data.
    if (length) {
        uint32_t word = 0;
        memcpy(&word, data, length);
        acc += Endian::networkToHostOrder<uint32_t>(word);
    }

    // Handle deferred carries.
    acc = (acc & 0xffffffff) + (acc >> 32);
    while (acc >> 16) {
        acc = (acc & 0xffff) + (acc >> 16);
    }

    // If the data began at an odd byte address
    // then reverse the byte order to compensate.
    if (offset & 1) {
        acc = ((acc & 0xff00) >> 8) | ((acc & 0x00ff) << 8);
    }

    headerChecksum(static_cast<uint16_t>(~acc));
}

unsigned short IPv4Header::getIHLFromByte(uint8_t &byte) {
    return (byte & (uint8_t) 0b1111) << 2;
}

uint8_t IPv4Header::ihl() const {
    if (_buffer->isBackwardMode())
        return (_buffer->get<uint8_t>(_comptPos<uint8_t>(0)) & (uint8_t) 0b1111) << 2;
    return (_buffer->get<uint8_t>(_startPosition) & (uint8_t) 0b1111) << 2;
}

void IPv4Header::ihl(uint8_t ihl) {
    uint8_t first4bit = _buffer->get<uint8_t>(_comptPos<uint8_t>(0)) & (uint8_t) 0b11110000;
    _buffer->put<uint8_t>(first4bit | (uint8_t) ((ihl >> 2) & 0b1111), _comptPos<uint8_t>(0));
}

uint8_t IPv4Header::typeOfService() const {
    return _buffer->get<uint8_t>(_comptPos<uint8_t>(1));
}

void IPv4Header::typeOfService(uint8_t typeOfService) {
    _buffer->put<uint8_t>(typeOfService, _comptPos<uint8_t>(1));
}

uint16_t IPv4Header::totalLength() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(2));
}

void IPv4Header::totalLength(uint16_t totalLength) {
    _buffer->put<uint16_t>(totalLength, _comptPos<uint16_t>(2));
}

uint16_t IPv4Header::identification() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(4));
}

void IPv4Header::identification(uint16_t identification) {
    _buffer->put<uint16_t>(identification, _comptPos<uint16_t>(4));
}

uint8_t IPv4Header::flags() const {
    return (_buffer->get<uint8_t>(_comptPos<uint8_t>(6)) & (uint8_t) 0b11100000) >> 5;
}

void IPv4Header::flags(uint8_t flags) {
    uint8_t left5Bit = _buffer->get<uint8_t>(_comptPos<uint8_t>(6)) & (uint8_t) 0b11111;
    _buffer->put<uint8_t>(left5Bit | (uint8_t) (flags & 0b111) << 5, _comptPos<uint8_t>(6));
}

uint16_t IPv4Header::fragmentOffset() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(6)) & (uint16_t) 0b1111111111111;
}

void IPv4Header::fragmentOffset(uint16_t fragmentOffset) {
    uint16_t first3bit = _buffer->get<uint16_t>(_comptPos<uint16_t>(6)) & ((uint16_t) 0b111 << 5 << 8);
    _buffer->put<uint16_t>(first3bit | (uint16_t) (fragmentOffset & 0b1111111111111), _comptPos<uint16_t>(6));
}

uint8_t IPv4Header::ttl() const {
    return _buffer->get<uint8_t>(_comptPos<uint8_t>(8));
}

void IPv4Header::ttl(uint8_t ttl) {
    _buffer->put<uint8_t>(ttl, _comptPos<uint8_t>(8));
}

IPNextProtocol IPv4Header::protocol() const {
    return IPNextProtocol(_buffer->get<uint8_t>(_comptPos<uint8_t>(9)));
}

void IPv4Header::protocol(IPNextProtocol protocol) {
    _buffer->put<uint8_t>(static_cast<uint8_t>(protocol), _comptPos<uint8_t>(9));
}

uint16_t IPv4Header::headerChecksum() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(10));
}

void IPv4Header::headerChecksum(uint16_t headerChecksum) {
    _buffer->put<uint16_t>(headerChecksum, _comptPos<uint16_t>(10));
}

uint32_t IPv4Header::sourceAddress() const {
    return _buffer->get<uint32_t>(_comptPos<uint32_t>(12));
}

void IPv4Header::sourceAddress(uint32_t sourceAddress) {
    _buffer->put<uint32_t>(sourceAddress, _comptPos<uint32_t>(12));
}

uint32_t IPv4Header::destAddress() const {
    return _buffer->get<uint32_t>(_comptPos<uint32_t>(16));
}

void IPv4Header::destAddress(uint32_t destAddress) {
    _buffer->put<uint32_t>(destAddress, _comptPos<uint32_t>(16));
}

const uint8_t *IPv4Header::ipOption() const {
    if (_buffer->isBackwardMode())
        return _buffer->buffer() + _buffer->size() - _startPosition - ipOptionLength();
    return _buffer->buffer() + _startPosition + 20;
}

void IPv4Header::ipOption(uint8_t *ipOption, uint8_t length) {
    ipOptionLength(length);
    if (_buffer->isBackwardMode())
        _buffer->putBytes(ipOption, length, _startPosition);
    else _buffer->putBytes(ipOption, length, _startPosition + 20);
}

const uint8_t IPv4Header::ipOptionLength() const {
    return ihl() - (uint8_t) 20;
}

void IPv4Header::ipOptionLength(uint8_t length) {
    ihl(length + (uint8_t) 20);
}
//
// IPv4Header
// ========================================================================================
//


//
// ========================================================================================
// IPv4HeaderBuilder
//
void IPv4HeaderBuilder::ihl(uint8_t v) {
    _ihl = v;
}

void IPv4HeaderBuilder::typeOfService(uint8_t v) {
    _typeOfService = v;
}

void IPv4HeaderBuilder::totalLength(uint16_t v) {
    _totalLength = v;
}

void IPv4HeaderBuilder::identification(uint16_t v) {
    _identification = v;
}

void IPv4HeaderBuilder::flags(uint8_t v) {
    _flags = v;
}

void IPv4HeaderBuilder::fragmentOffset(uint16_t v) {
    _fragmentOffset = v;
}

void IPv4HeaderBuilder::ttl(uint8_t v) {
    _ttl = v;
}

void IPv4HeaderBuilder::protocol(IPNextProtocol v) {
    _protocol = v;
}

void IPv4HeaderBuilder::headerChecksum(uint16_t v) {
    _headerChecksum = v;
    _isChecksumSet = true;
}

void IPv4HeaderBuilder::sourceAddress(uint32_t v) {
    _sourceAddress = v;
}

void IPv4HeaderBuilder::destAddress(uint32_t v) {
    _destAddress = v;
}

void IPv4HeaderBuilder::ipOption(uint8_t *v, uint8_t len) {
    _ipOption = v;
    _ipOptionLength = len;
}

std::shared_ptr<IPv4Header> IPv4HeaderBuilder::build(NetworkBufferPointer buffer, unsigned short offset) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    if (_totalLength == 0 || _sourceAddress == 0 || _destAddress == 0)
        throw std::invalid_argument("Total length, source address, and dest address must all be set");

    if (buffer->size() < offset + _ihl) {
        auto writePos = buffer->getWritePos();
        auto readPos = buffer->getReadPos();
        buffer->resize(offset + _ihl);
        buffer->setWritePos(writePos);
        buffer->setReadPos(readPos);
    }

    auto ipv4Header = std::make_shared<IPv4Header>(buffer, offset, _ihl);
    ipv4Header->version(IPVersion::Version4);
    ipv4Header->ihl(_ihl);
    ipv4Header->totalLength(_totalLength);
    ipv4Header->identification(_identification);
    ipv4Header->flags(_flags);
    ipv4Header->fragmentOffset(_fragmentOffset);
    ipv4Header->ttl(_ttl);
    ipv4Header->protocol(_protocol);
    ipv4Header->headerChecksum(_headerChecksum);
    ipv4Header->sourceAddress(_sourceAddress);
    ipv4Header->destAddress(_destAddress);

    if (!_isChecksumSet) {
        ipv4Header->updateChecksum();
    }

    if (buffer->getWritePos() < offset + _ihl)
        buffer->setWritePos(offset + _ihl);

    return ipv4Header;
}
