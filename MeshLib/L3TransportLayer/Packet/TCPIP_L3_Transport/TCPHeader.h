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

#ifndef ANYFIMESH_CORE_TCPHEADER_H
#define ANYFIMESH_CORE_TCPHEADER_H

#include <list>
#include "TCPIPL3.h"

#include "../../../Common/Network/Address.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20


// Forward declarations : Address
namespace Anyfi {
class Address;
};

// Forward declarations : this
class TCPHeader;
class TCPHeaderBuilder;
class TCPOption;
class SimpleTCPOption;


class TCPHeader : public TCPIP::L3Header {
public:
    TCPHeader(const NetworkBufferPointer &buffer) : TCPIP::L3Header(buffer) {}
    TCPHeader(const NetworkBufferPointer &buffer, unsigned short startPosition, unsigned short length = 0)
            : TCPIP::L3Header(buffer, startPosition), _len(length) {}

    /**
     * Returns a length of the packet header.
     */
    unsigned short length() const override {
        if (_len != 0) return _len;
        return offset();
    }

    /**
     * Calculate a TCPIP L3 header checksum, and update the checksum field.
     */
    void updateChecksum(IPHeader *ipHeader) override;
    void updateIPv4Checksum(uint32_t srcAddr, uint32_t dstAddr, uint32_t tcpLength) override;

    /**
     * TCP flag check methods
     */
    bool isURG() const;
    bool isACK() const;
    bool isPSH() const;
    bool isRST() const;
    bool isSYN() const;
    bool isFIN() const;

//
// ========================================================================================
// TCP header fields
//
    uint16_t sourcePort() const;
    void sourcePort(uint16_t sourcePort);

    uint16_t destPort() const;
    void destPort(uint16_t destPort);

    uint32_t seqNum() const;
    void seqNum(uint32_t seqNum);

    uint32_t ackNum() const;
    void ackNum(uint32_t ackNum);

    uint16_t offset() const;
    void offset(uint16_t offset);

    uint8_t reserved() const;
    void reserved(uint8_t reserved);

    uint8_t flags() const;
    void flags(uint8_t flags);

    uint16_t window() const;
    void window(uint16_t window);

    uint16_t checksum() const;
    void checksum(uint16_t checksum);

    uint16_t urgentPointer() const;
    void urgentPointer(uint16_t urgentPointer);

    const uint8_t *tcpRawOptions() const;
    void tcpRawOptions(uint8_t *tcpOption, uint8_t length);
    const std::vector<TCPOption> tcpOptions();
    void tcpOptions(std::vector<SimpleTCPOption> options);

    uint16_t maxSegmentSize() const;
    uint8_t windowScale() const;
    uint32_t tsVal() const;
    uint32_t tsEcr() const;


    const uint16_t tcpOptionLength() const;
    void tcpOptionLength(uint16_t length);
//
// TCP header fields
// ========================================================================================
//

private:
    unsigned short _len;

    uint16_t _mss = 0;
    uint8_t _ws = 0;
    uint32_t _tsVal = 0;
    uint32_t _tsEcr = 0;
};


class TCPHeaderBuilder : public PacketHeaderBuilder<TCPHeader> {
public:
    /**
     * TCP header fields
     */
    void sourcePort(uint16_t sourcePort);
    void destPort(uint16_t destPort);
    void seqNum(uint32_t seqNum);
    void ackNum(uint32_t ackNum);
    void offset(uint16_t offset);
    void reserved(uint8_t reserved);
    void flags(uint8_t flags);
    void window(uint16_t window);
    void checksum(uint16_t checksum);
    void urgentPointer(uint16_t urgentPointer);
    void tcpRawOption(uint8_t *tcpOption, uint8_t length);
    void tcpOptions(std::vector<SimpleTCPOption> options);
    void tcpOptionLength(uint16_t length);

    /**
     * Checksum pseudo header info
     */
    void srcAddr(uint32_t addr);
    void dstAddr(uint32_t addr);
    void tcpLength(uint32_t tcpLen);

    void srcAddr(uint32_t addr, uint16_t port);
    void dstAddr(uint32_t addr, uint16_t port);

    std::shared_ptr<TCPHeader> build(NetworkBufferPointer buffer, unsigned short offset = 0) override;

private:
    /**
     * TCP header fields
     */
    uint16_t _sourcePort = 0;
    uint16_t _destPort = 0;
    uint32_t _seqNum = 0;
    uint32_t _ackNum = 0;
    uint16_t _offset = 20;
    uint8_t _reserved = 0;
    uint8_t _flags = 0;
    uint16_t _window = 0;
    uint16_t _checksum = 0;
    uint16_t _urgentPointer = 0;
    uint8_t *_tcpRawOption = nullptr;
    std::vector<SimpleTCPOption> _tcpOptions;
    uint8_t _tcpOptionLength = 0;

    /**
     * Checksum pseudo header info
     */
    uint32_t _srcAddr = 0;
    uint32_t _dstAddr = 0;
    uint32_t _tcpLength = 0;
};


/**
 * The length of this field is determined by the data offset field.
 * Options have up three fields : Option-Kind(1 byte), Option-Length(1 byte), Option-Data (Variable).
 * The Option-Kind fields indicates the type of option, and is the only dfield that is not optional.
 * Depending on what kind of option we are dealing with, the next two fields may be set:.
 * the Option-Length field indicates the total length of the option, and the Option-Data field contains
 * the value of the option, if applicable.
 */
class TCPOption : public PacketHeader {
public:
    /**
     * Comment format
     * > (Option-Kind, Option-Length, Option-Data)
     */
    enum Kind : uint8_t {
        /**
         * (0) 8 bits
         * End of Option List.
         */
        EOL             = 0,

        /**
         * (1) 8 bits
         * No operation (NOP, Padding).
         * This may be used to align option fields on 32-bit boundaries for better performance.
         */
        NOP             = 1,

        /**
         * (2, 4, SS) 32 bits
         * Maximum segment size.
         */
        MSS             = 2,

        /**
         * (3, 3, S) 24 bits
         * Window scale.
         */
        WS              = 3,

        /**
         * (4, 2) 16 bits
         * Selective Acknowledgement permitted.
         */
        SACK_PERMITTED  = 4,

        /**
         * (5, N, BBBB, EEEE, ...) variable bits, N is either 10, 18, 26, or 34
         * Selective Acknowledgement.
         * These first two bytes are followed by a list of 1-4 blocks being selectively acknowledged,
         * specified as 32-bit begin/end pointers.
         */
        SACK            = 5,

        /**
         * (8, 10, TTTT, EEEE) 80bits
         * Timestamp and echo of previous timestamp.
         */
        TIMESTAMP       = 8,
    };
    enum LenOfKind : uint8_t {
        LEN_EOL             =  1,
        LEN_NOP             =  1,
        LEN_MSS             =  4,
        LEN_WS              =  3,
        LEN_SACK_PERMITTED  =  2,
        LEN_SACK            =  0,    /* Not fixed value */
        LEN_TIMESTAMP       = 10,
    };

    /**
     * TCPOption constructor for forward NetworkBuffer.
     *
     * @param buffer
     * @param offset
     */
    TCPOption(NetworkBufferPointer buffer, unsigned short offset)
            : PacketHeader(buffer, offset) {
        if (buffer->isBackwardMode())
            throw std::invalid_argument("This TCPOption constructor is not for backward networkMode.");
        _kind = kind();
    }
    /**
     * TCPOption constructor for both of forward and backward NetworkBuffer
     * @param buffer
     * @param offset
     * @param kind
     */
    TCPOption(NetworkBufferPointer buffer, unsigned short offset, Kind kind)
            : PacketHeader(buffer, offset), _kind(kind) {}

    /**
     * Returns a length of this option.
     */
    unsigned short length() const override;

    /**
     * Returns whether this option is significant.
     * Significant options are handled in the Packet Processor.
     */
    bool isSignificant() const;

    /**
     * Returns a corresponding length of the given kind.
     */
    static uint8_t lenOfKind(Kind kind);
    /**
     * Returns number of values.
     */
    static unsigned char numValuesOfKind(Kind kind);

    Kind kind() const;
    void kind(Kind kind);

    uint32_t value1();
    void value1(uint32_t value);

    uint32_t value2();
    void value2(uint32_t value);

private:
    Kind _kind;

    void _len(uint8_t length);

    /**
     * Check if the given kind and value are appropriate.
     * If not valid, throw exception.
     */
    void _checkKindAndValue(Kind &kind, uint32_t value1, uint32_t value2) const;
};

class SimpleTCPOption {
public:
    SimpleTCPOption(const TCPOption::Kind _kind)
            : _kind(_kind) {}
    SimpleTCPOption(const TCPOption::Kind _kind, const uint32_t _value1, const uint32_t _value2 = 0)
            : _kind(_kind), _value1(_value1), _value2(_value2) {}

    bool operator==(TCPOption t) const;


    TCPOption::Kind kind() const { return _kind; }
    uint32_t value1() const { return _value1; }
    uint32_t value2() const { return _value2; };

private:
    const TCPOption::Kind _kind;
    const uint32_t _value1 = 0;
    const uint32_t _value2 = 0;
};

#endif //ANYFIMESH_CORE_TCPHEADER_H
