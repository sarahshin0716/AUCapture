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

#include "IPv6Header.h"

IPv6Header::IPv6Header(const NetworkBufferPointer &buffer) : IPHeader(buffer) {}

IPv6Header::IPv6Header(const NetworkBufferPointer &buffer, unsigned short offset) : IPHeader(buffer, offset) {

}

uint16_t IPv6Header::trafficClass() const {
    return (_buffer->get<uint16_t>(_comptPos<uint16_t>(0)) & (uint16_t) 0b111111110000) >> 4;
}

void IPv6Header::trafficClass(uint16_t trafficClass) {
    uint16_t left4Bit = _buffer->get<uint16_t>(_comptPos<uint16_t>(0)) & (uint16_t) 0b1111000000000000;
    uint16_t right4Bit = _buffer->get<uint16_t>(_comptPos<uint16_t>(0)) & (uint16_t) 0b1111;
    _buffer->put<uint16_t>(left4Bit | right4Bit | (uint16_t) (trafficClass & 0b11111111) << 4, _comptPos<uint16_t>(0));
}

uint32_t IPv6Header::flowLabel() const {
    return _buffer->get<uint32_t>(_comptPos<uint32_t>(0)) & (uint32_t) 0b11111111111111111111;
}

void IPv6Header::flowLabel(uint32_t flowLabel) {
    uint32_t first12bit = _buffer->get<uint32_t>(_comptPos<uint32_t>(0)) & ((uint32_t) 0b111111111111 << 20);
    _buffer->put<uint32_t>(first12bit | (uint32_t) (flowLabel & 0b11111111111111111111), _comptPos<uint32_t>(0));
}


uint16_t IPv6Header::payloadLength() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(4));
}

void IPv6Header::payloadLength(uint16_t payloadLength) {
    _buffer->put<uint16_t>(payloadLength, _comptPos<uint16_t>(4));
}

IPNextProtocol IPv6Header::nextHeader() const {
    return IPNextProtocol(_buffer->get<uint8_t>(_comptPos<uint8_t>(6)));
}

void IPv6Header::nextHeader(IPNextProtocol nextHeader) {
    _buffer->put<uint8_t>(static_cast<uint8_t>(nextHeader), _comptPos<uint8_t>(6));
}

uint8_t IPv6Header::hopLimit() const {
    return _buffer->get<uint8_t>(_comptPos<uint8_t>(7));
}

void IPv6Header::hopLimit(uint8_t nextHeader) {
    _buffer->put<uint8_t>(nextHeader, _comptPos<uint8_t>(7));
}

std::vector<uint32_t> IPv6Header::sourceAddress() const {
    std::vector<uint32_t> sourceAddress;
    sourceAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(8)));
    sourceAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(12)));
    sourceAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(16)));
    sourceAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(20)));
    return sourceAddress;
}

void IPv6Header::sourceAddress(std::vector<uint32_t> sourceAddress) {
    _buffer->put<uint32_t>(sourceAddress.at(0), _comptPos<uint32_t>(8));
    _buffer->put<uint32_t>(sourceAddress.at(1), _comptPos<uint32_t>(12));
    _buffer->put<uint32_t>(sourceAddress.at(2), _comptPos<uint32_t>(16));
    _buffer->put<uint32_t>(sourceAddress.at(3), _comptPos<uint32_t>(20));
}

std::vector<uint32_t> IPv6Header::destAddress() const {
    std::vector<uint32_t> destAddress;
    destAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(24)));
    destAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(28)));
    destAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(32)));
    destAddress.push_back(_buffer->get<uint32_t>(_comptPos<uint32_t>(36)));
    return destAddress;
}

void IPv6Header::destAddress(std::vector<uint32_t> destAddress) {
    _buffer->put<uint32_t>(destAddress.at(0), _comptPos<uint32_t>(24));
    _buffer->put<uint32_t>(destAddress.at(1), _comptPos<uint32_t>(28));
    _buffer->put<uint32_t>(destAddress.at(2), _comptPos<uint32_t>(32));
    _buffer->put<uint32_t>(destAddress.at(3), _comptPos<uint32_t>(36));
}