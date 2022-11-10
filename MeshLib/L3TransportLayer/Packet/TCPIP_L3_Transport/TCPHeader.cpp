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

#include "../../../L3TransportLayer/Packet/TCPIP_L2_Internet/IPv4Header.h"
#include "../../../Common/Network/Address.h"
#include "TCPHeader.h"


#define IPPROTO_TCP 6


//
// ========================================================================================
// TCPHeader
//
uint16_t tcp_checksum(const void *buff, size_t len, uint32_t src_addr, uint32_t dest_addr) {
    const uint16_t *buf = (uint16_t *) buff;
    uint16_t *ip_src = (uint16_t *) &src_addr, *ip_dst = (uint16_t *) &dest_addr;
    uint32_t sum;
    size_t length = len;

    // Calculate the sum
    sum = 0;
    while (len > 1) {
        sum += *buf++;
        if (sum & 0x80000000)
            sum = (sum & 0xFFFF) + (sum >> 16);
        len -= 2;
    }

    if (len & 1)
        // Add the padding if the packet lenght is odd
        sum += *((uint8_t *) buf);

    // Add the pseudo-header
    sum += *(ip_src++);
    sum += *ip_src;
    sum += *(ip_dst++);
    sum += *ip_dst;
    sum += Endian::hostToNetworkOrder<uint16_t>(IPPROTO_TCP);
    sum += Endian::hostToNetworkOrder<uint16_t>((uint16_t) length);

    // Add the carries
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    // Return the one's complement of sum
    return ((uint16_t) (~sum));
}

void TCPHeader::updateChecksum(IPHeader *ipHeader) {
    if (ipHeader->version() == IPVersion::Version4) {
        IPv4Header *ipv4Header = dynamic_cast<IPv4Header *>(ipHeader);
        updateIPv4Checksum(ipv4Header->sourceAddress(), ipv4Header->destAddress(),
                           ipv4Header->totalLength() - ipv4Header->ihl());
    } else {
        // Not yet supported
    }
}

void TCPHeader::updateIPv4Checksum(uint32_t srcAddr, uint32_t dstAddr, uint32_t tcpLength) {
    checksum(0);

    uint8_t *p;
    if (_buffer->isBackwardMode())
        p = _buffer->buffer() + _buffer->size() - _startPosition - length();
    else p = _buffer->buffer() + _startPosition;
    checksum(Endian::networkToHostOrder<uint16_t>(tcp_checksum(p, tcpLength,
                                                               Endian::hostToNetworkOrder<uint32_t>(srcAddr),
                                                               Endian::hostToNetworkOrder<uint32_t>(dstAddr))));
}

bool TCPHeader::isURG() const {
    return (flags() & TCP_FLAG_URG) == TCP_FLAG_URG;
}

bool TCPHeader::isACK() const {
    return (flags() & TCP_FLAG_ACK) == TCP_FLAG_ACK;
}

bool TCPHeader::isPSH() const {
    return (flags() & TCP_FLAG_PSH) == TCP_FLAG_PSH;
}

bool TCPHeader::isRST() const {
    return (flags() & TCP_FLAG_RST) == TCP_FLAG_RST;
}

bool TCPHeader::isSYN() const {
    return (flags() & TCP_FLAG_SYN) == TCP_FLAG_SYN;
}

bool TCPHeader::isFIN() const {
    return (flags() & TCP_FLAG_FIN) == TCP_FLAG_FIN;
}

uint16_t TCPHeader::sourcePort() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(0));
}

void TCPHeader::sourcePort(uint16_t sourcePort) {
    _buffer->put<uint16_t>(sourcePort, _comptPos<uint16_t>(0));
}

uint16_t TCPHeader::destPort() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(2));
}

void TCPHeader::destPort(uint16_t destPort) {
    _buffer->put<uint16_t>(destPort, _comptPos<uint16_t>(2));
}

uint32_t TCPHeader::seqNum() const {
    return _buffer->get<uint32_t>(_comptPos<uint32_t>(4));
}

void TCPHeader::seqNum(uint32_t seqNum) {
    _buffer->put<uint32_t>(seqNum, _comptPos<uint32_t>(4));
}

uint32_t TCPHeader::ackNum() const {
    return buffer()->get<uint32_t>(_comptPos<uint32_t>(8));
}

void TCPHeader::ackNum(uint32_t ackNum) {
    _buffer->put<uint32_t>(ackNum, _comptPos<uint32_t>(8));
}

uint16_t TCPHeader::offset() const {
    if (_buffer->isBackwardMode())
        return (_buffer->get<uint8_t>(_comptPos<uint8_t>(12)) & (uint8_t) 0b11110000) >> 2;
    else return (_buffer->get<uint8_t>(_startPosition + 12) & (uint8_t) 0b11110000) >> 2;
}

void TCPHeader::offset(uint16_t offset) {
    _len = offset;
    uint8_t last4bit = _buffer->get<uint8_t>(_comptPos<uint8_t>(12, offset)) & (uint8_t) 0b1111;
    _buffer->put<uint8_t>(((uint8_t) ((offset >> 2) & 0b1111) << 4) | last4bit, _comptPos<uint8_t>(12, offset));
}

uint8_t TCPHeader::reserved() const {
    return _buffer->get<uint8_t>(_comptPos<uint8_t>(12)) & (uint8_t) 0b1111;
}

void TCPHeader::reserved(uint8_t reserved) {
    uint8_t first4bit = _buffer->get<uint8_t>(_comptPos<uint8_t>(12)) & (uint8_t) 0b11110000;
    _buffer->put<uint8_t>(first4bit | (reserved & (uint8_t) 0b1111), _comptPos<uint8_t>(12));
}

uint8_t TCPHeader::flags() const {
    return _buffer->get<uint8_t>(_comptPos<uint8_t>(13));
}

void TCPHeader::flags(uint8_t flags) {
    _buffer->put<uint8_t>(flags, _comptPos<uint8_t>(13));
}

uint16_t TCPHeader::window() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(14));
}

void TCPHeader::window(uint16_t window) {
    _buffer->put<uint16_t>(window, _comptPos<uint16_t>(14));
}

uint16_t TCPHeader::checksum() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(16));
}

void TCPHeader::checksum(uint16_t checksum) {
    _buffer->put<uint16_t>(checksum, _comptPos<uint16_t>(16));
}

uint16_t TCPHeader::urgentPointer() const {
    return _buffer->get<uint16_t>(_comptPos<uint16_t>(18));
}

void TCPHeader::urgentPointer(uint16_t urgentPointer) {
    _buffer->put<uint16_t>(urgentPointer, _comptPos<uint16_t>(18));
}

const uint8_t *TCPHeader::tcpRawOptions() const {
    if (_buffer->isBackwardMode())
        return _buffer->buffer() + _buffer->size() - _startPosition - tcpOptionLength();
    return _buffer->buffer() + _startPosition + 20;
}

void TCPHeader::tcpRawOptions(uint8_t *tcpOption, uint8_t length) {
    tcpOptionLength(length);
    if (_buffer->isBackwardMode())
        memcpy(_buffer->buffer() + _buffer->size() - _startPosition - tcpOptionLength(), tcpOption, length);
    else
        memcpy(_buffer->buffer() + _startPosition + 20, tcpOption, length);
}

const std::vector<TCPOption> TCPHeader::tcpOptions() {
    std::vector<TCPOption> options;

    auto len = tcpOptionLength();
    unsigned short offset = 0;
    while (offset < len) {
        TCPOption *option;
        if (_buffer->isBackwardMode()) {
            TCPOption::Kind kind = static_cast<TCPOption::Kind>(_buffer->get<uint8_t>(_comptPos<uint8_t>(20 + offset)));
            option = new TCPOption(_buffer, _comptPos(20 + offset, TCPOption::lenOfKind(kind), length()), kind);
        } else {
            option = new TCPOption(_buffer, _startPosition + 20 + offset);
        }

        offset += option->length();

        if (option->isSignificant()) {
            if (option->kind() == TCPOption::Kind::MSS)
                _mss = (uint16_t) option->value1();
            else if (option->kind() == TCPOption::Kind::WS)
                _ws = (uint8_t) option->value1();
            else if (option->kind() == TCPOption::Kind::TIMESTAMP) {
                _tsVal = option->value1();
                _tsEcr = option->value2();
            }
            options.push_back(*option);
        }
        delete option;
    }

    return options;
}

void TCPHeader::tcpOptions(std::vector<SimpleTCPOption> options) {
    // Fill NOP
    unsigned short totalLength = 0;
    for (auto &option : options) {
        totalLength += TCPOption::lenOfKind(option.kind());
    }
    auto left = (4 - totalLength % 4) % 4;
    for (int i = 0; i < left; ++i) {
        options.emplace_back(TCPOption::Kind::NOP);
        totalLength++;
    }

    // Set tcp option length
    tcpOptionLength(totalLength);

    // Set tcp options
    unsigned short offset = 0;
    for (auto &option : options) {
        TCPOption opt(_buffer, _comptPos(20 + offset, TCPOption::lenOfKind(option.kind()), length()), option.kind());
        opt.kind(option.kind());

        unsigned short num = TCPOption::numValuesOfKind(option.kind());
        if (num >= 1) opt.value1(option.value1());
        if (num >= 2) opt.value2(option.value2());

        offset += opt.length();
    }
}

uint16_t TCPHeader::maxSegmentSize() const {
    return _mss;
}

uint8_t TCPHeader::windowScale() const {
    return _ws;
}

uint32_t TCPHeader::tsVal() const {
    return _tsVal;
}

uint32_t TCPHeader::tsEcr() const {
    return _tsEcr;
}

const uint16_t TCPHeader::tcpOptionLength() const {
    return offset() - (uint8_t) 20;
}

void TCPHeader::tcpOptionLength(uint16_t length) {
    offset((uint8_t) 20 + length);
}
//
// TCPHeader
// ========================================================================================
//


//
// ========================================================================================
// TCPHeaderBuilder
//
void TCPHeaderBuilder::sourcePort(uint16_t sourcePort) {
    _sourcePort = sourcePort;
}

void TCPHeaderBuilder::destPort(uint16_t destPort) {
    _destPort = destPort;
}

void TCPHeaderBuilder::seqNum(uint32_t seqNum) {
    _seqNum = seqNum;
}

void TCPHeaderBuilder::ackNum(uint32_t ackNum) {
    _ackNum = ackNum;
}

void TCPHeaderBuilder::offset(uint16_t offset) {
    _offset = offset;
}

void TCPHeaderBuilder::reserved(uint8_t reserved) {
    _reserved = reserved;
}

void TCPHeaderBuilder::flags(uint8_t flags) {
    _flags = flags;
}

void TCPHeaderBuilder::window(uint16_t window) {
    _window = window;
}

void TCPHeaderBuilder::checksum(uint16_t checksum) {
    _checksum = checksum;
}

void TCPHeaderBuilder::urgentPointer(uint16_t urgentPointer) {
    _urgentPointer = urgentPointer;
}

void TCPHeaderBuilder::tcpRawOption(uint8_t *tcpOption, uint8_t length) {
    _tcpRawOption = tcpOption;
    _tcpOptionLength = length;
}

void TCPHeaderBuilder::tcpOptions(std::vector<SimpleTCPOption> options) {
    _tcpOptions.clear();
    for (auto &option : options) {
        _tcpOptions.push_back(option);
    }

    _tcpOptionLength = 0;
    for (auto &option : options) {
        _tcpOptionLength += TCPOption::lenOfKind(option.kind());
    }
    if (_tcpOptionLength % 4 != 0)
        _tcpOptionLength += 4 - _tcpOptionLength % 4;
}

void TCPHeaderBuilder::tcpOptionLength(uint16_t length) {
    _tcpOptionLength = static_cast<uint8_t>(length);
}

void TCPHeaderBuilder::srcAddr(uint32_t addr) {
    _srcAddr = addr;
}

void TCPHeaderBuilder::dstAddr(uint32_t addr) {
    _dstAddr = addr;
}

void TCPHeaderBuilder::tcpLength(uint32_t tcpLen) {
    _tcpLength = tcpLen;
}

void TCPHeaderBuilder::srcAddr(uint32_t addr, uint16_t port) {
    _srcAddr = addr;
    _sourcePort = port;
}

void TCPHeaderBuilder::dstAddr(uint32_t addr, uint16_t port) {
    _dstAddr = addr;
    _destPort = port;
}

std::shared_ptr<TCPHeader> TCPHeaderBuilder::build(NetworkBufferPointer buffer, unsigned short offset) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    if (buffer->size() < offset + _offset) {
        auto writePos = buffer->getWritePos();
        auto readPos = buffer->getReadPos();
        buffer->resize(offset + _offset);
        buffer->setWritePos(writePos);
        buffer->setReadPos(readPos);
    }

    auto tcpHeader = std::make_shared<TCPHeader>(buffer, offset, _offset);
    tcpHeader->sourcePort(_sourcePort);
    tcpHeader->destPort(_destPort);
    tcpHeader->seqNum(_seqNum);
    tcpHeader->ackNum(_ackNum);
    tcpHeader->offset(_offset);
    tcpHeader->reserved(_reserved);
    tcpHeader->flags(_flags);
    tcpHeader->window(_window);
    tcpHeader->checksum(_checksum);
    tcpHeader->urgentPointer(_urgentPointer);
    if (_tcpRawOption != nullptr && _tcpOptionLength != 0)
        tcpHeader->tcpRawOptions(_tcpRawOption, _tcpOptionLength);
    if (!_tcpOptions.empty() && _tcpOptionLength != 0)
        tcpHeader->tcpOptions(_tcpOptions);

    if (_checksum == 0) {
        if (_srcAddr == 0 || _dstAddr == 0 || _tcpLength == 0)
            throw std::invalid_argument("TCP Checksum pseudo info required");
        tcpHeader->updateIPv4Checksum(_srcAddr, _dstAddr, _tcpLength);
    }

    if (buffer->getWritePos() < offset + _offset)
        buffer->setWritePos(offset + _offset);

    return tcpHeader;
}
//
// TCPHeaderBuilder
// ========================================================================================
//


//
// ========================================================================================
// TCPOption
//
unsigned short TCPOption::length() const {
    return lenOfKind(_kind);
}

bool TCPOption::isSignificant() const {
    switch (kind()) {
        case MSS:
        case WS:
        case SACK_PERMITTED:
        case SACK:
        case TIMESTAMP:
            return true;
        case EOL:
        case NOP:
        default:
            return false;
    }
}

uint8_t TCPOption::lenOfKind(TCPOption::Kind kind) {
    switch (kind) {
        case EOL:
            return LEN_EOL;
        case NOP:
            return LEN_NOP;
        case MSS:
            return LEN_MSS;
        case WS:
            return WS;
        case SACK_PERMITTED:
            return LEN_SACK_PERMITTED;
        case SACK:
            return 0; // flexible length
        case TIMESTAMP:
            return LEN_TIMESTAMP;
        default:
            throw std::invalid_argument("Invalid kind");
    }
}

unsigned char TCPOption::numValuesOfKind(TCPOption::Kind kind) {
    switch (kind) {
        case EOL:
        case NOP:
        case SACK_PERMITTED:
            return 0;
        case MSS:
        case WS:
            return 1;
        case SACK:
            return 0; // flexible num of values
        case TIMESTAMP:
            return 2;
        default:
            throw std::invalid_argument("Invalid kind");
    }
}

TCPOption::Kind TCPOption::kind() const {
    if (_buffer->isBackwardMode()) {
        return TCPOption::Kind(_buffer->get<uint8_t>(_comptPos<uint8_t>(0)));
    } else {
        return TCPOption::Kind(_buffer->get<uint8_t>(_startPosition));
    }
}

void TCPOption::kind(TCPOption::Kind kind) {
    _buffer->put<uint8_t>((uint8_t) kind, _comptPos<uint8_t>(0));

    auto l = lenOfKind(kind);
    if (l > 1)
        _len(l);
}

void TCPOption::_len(uint8_t length) {
    _buffer->put<uint8_t>(length, _comptPos<uint8_t>(1));
}

uint32_t TCPOption::value1() {
    Kind k = kind();
    auto size = (lenOfKind(k) - 2) / numValuesOfKind(k);

    uint32_t val = 0;
    if (size == 1) val = _buffer->get<uint8_t>(_comptPos<uint8_t>(2));
    else if (size == 2) val = _buffer->get<uint16_t>(_comptPos<uint16_t>(2));
    else if (size == 4) val = _buffer->get<uint32_t>(_comptPos<uint32_t>(2));

    _checkKindAndValue(k, val, 0);
    return val;
}

void TCPOption::value1(uint32_t value) {
    _checkKindAndValue(_kind, value, 0);

    auto size = (lenOfKind(_kind) - 2) / numValuesOfKind(_kind);
    if (size == 1) _buffer->put<uint8_t>(static_cast<uint8_t>(value), _comptPos<uint8_t>(2));
    else if (size == 2) _buffer->put<uint16_t>(static_cast<uint16_t>(value), _comptPos<uint16_t>(2));
    else if (size == 4) _buffer->put<uint32_t>(static_cast<uint32_t>(value), _comptPos<uint32_t>(2));
}

uint32_t TCPOption::value2() {
    auto size = (lenOfKind(_kind) - 2) / numValuesOfKind(_kind);

    uint32_t val = 0;
    if (size == 1) val = _buffer->get<uint8_t>(_comptPos<uint8_t>(2 + size));
    else if (size == 2) val = _buffer->get<uint16_t>(_comptPos<uint16_t>(2 + size));
    else if (size == 4) val = _buffer->get<uint32_t>(_comptPos<uint32_t>(2 + size));

    _checkKindAndValue(_kind, 0, val);
    return val;
}

void TCPOption::value2(uint32_t value) {
    _checkKindAndValue(_kind, 0, value);

    auto size = (lenOfKind(_kind) - 2) / numValuesOfKind(_kind);
    if (size == 1) _buffer->put<uint8_t>(static_cast<uint8_t>(value), _comptPos<uint8_t>(2 + size));
    else if (size == 2) _buffer->put<uint16_t>(static_cast<uint16_t>(value), _comptPos<uint16_t>(2 + size));
    else if (size == 4) _buffer->put<uint32_t>(static_cast<uint32_t>(value), _comptPos<uint32_t>(2 + size));
}

void TCPOption::_checkKindAndValue(TCPOption::Kind &kind, uint32_t value1, uint32_t value2) const {
    switch (kind) {
        case EOL:
        case NOP:
        case SACK_PERMITTED:
            throw std::invalid_argument("EOL, NOP, or SACK_PERMITTED can not set or get value");
        case MSS:
            if (value2 != 0)
                throw std::invalid_argument("MSS can not set or get value 2");
            if (value1 > UINT16_MAX)
                throw std::invalid_argument("MSS value should be uint16_t");
            break;
        case WS:
            if (value2 != 0)
                throw std::invalid_argument("WS can not set or get value 2");
            if (value1 > UINT8_MAX)
                throw std::invalid_argument("WS value should be uint8_t");
            break;
        case SACK:
            // Not impl in this version.
            break;
        case TIMESTAMP:
            // Always correct
            break;
    }
}
//
// TCPOption
// ========================================================================================
//


//
// ========================================================================================
// SimpleTCPOption
//
bool SimpleTCPOption::operator==(TCPOption t) const {
    bool equal = t.kind() == _kind;
    if (!equal) return equal;

    auto num = t.numValuesOfKind(_kind);
    if (num == 0) return equal;
    if (num >= 1) {
        equal = t.value1() == _value1;
        if (!equal) return equal;
    }
    if (num >= 2) {
        equal = t.value2() == _value2;
    }
    return equal;
}
//
// SimpleTCPOption
// ========================================================================================
//
