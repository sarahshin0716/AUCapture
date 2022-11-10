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

#ifndef ANYFIMESH_CORE_RUDPHEADER_H
#define ANYFIMESH_CORE_RUDPHEADER_H

#include "MeshProtocolHeader.h"


#define MESH_RUDP_HEADER_LENGTH 16


enum MeshRUDPFlag : uint8_t {
    /**
     * Last packet from sender.
     */
    FIN = 0b00000001,

    /**
     * Synchronize sequence numbers. Only the first packet sent from each end should have this flag set.
     * Some other flags and fields change meaning based on this flag,
     * and some are only valid for when it is set, and others when it is clear.
     */
    SYN = 0b00000010,

    /**
     * Reset the connection
     */
    RST = 0b00000100,

    /**
     * The EACK bit indicates an extended acknowledge segment is present.
     * The EACK segment is used to acknowledge segments received out of sequence. It contains the sequence numbers
     * of one or more segments received out of sequence. The EACK is always combined with an ACK in the segment,
     * giving the sequence number of the last segment received in sequence. The header length is variable for the
     * EACK segment. Its minimum value is seven and its maximum value depends on the maximum receive queue length.
     */
    EAK = 0b00001000,

    /**
     * Indicates that the Acknowledgement field is significant.
     * All packets after the initial SYN packet sent by the client should have this flag set.
     */
    ACK = 0b00010000,

    /**
     * [Not used in this version]
     * Indicates that the Urgent pointer field is significant.
     */
    URG = 0b00100000,

    /**
     * [Not used in this version]
     * ECN-Echo has a dual role, depending on the value of the SYN flag. It indicates :
     * 1. If the SYN flag is set (1), that the TCP peer is ECN capable.
     * 2. If the SYN flag is clear (0), that a packet with Congestion Experienced flag set (ECN=11)
     *   in IP header was received during normal transmission. This serves as an indication
     *   of network congestion to the RUDP sender.
     */
    ECE = 0b01000000,

    /**
     * [Not used in this version]
     * Congestion Window Reduced (CWR) flag is set by sending host to indicate that it received
     * a RUDP segment with the ECE flag set and had responded in congestion control mechanism.
     */
    CWR = 0b10000000,
};

/**
 * Fields in AnyfiRUDPHeader
 *  Bits || 0 - - - | 4 - - - || 8 - - - | 12- - - || 16- - - | 20- - - || 24- - - | 28- - - ||
 * Bytes || 0 - - - | - - - - || 1 - - - | - - - - || 2 - - - | - - - - || 3 - - - | - - - - ||
 * ===== || ================================================================================ ||
 *     0 ||   Anyfi Protocol  ||     RUDP Flags    ||               Window Size              ||
 *     4 ||       Anyfi Transport Source Port      ||    Anyfi Transport Destination Port    ||
 *     8 ||                                  Sequence Number                                 ||
 *    12 ||                             Acknowledgement Number                               ||
 * bytes || ================================================================================ ||
 *
 * Unique fields in AnyfiRUDPHeader
 * - RUDP Flags : See MeshRUDPFlag.
 * - Window Size : The size of the receive window, which specifies the number of window size units (by default, bytes).
 * - Sequence Number : Has a dual role.
 *                    1. If the SYN flag is set (1), then this is initial sequence number.
 *                      The sequence number for the actual first data byte and the acknowledged number
 *                      in the corresponding ACK are then this sequence number plus 1.
 *                    2. If the SYN flag is clear (0), then this is the accumulated sequence number
 *                      of the first data byte of this segment for the current session.
 * - Acknowledgement Number : If the ACK flag is set then the value of this field is the next sequence number
 *                           that the sender of the ACK is expecting. The acknowledges receipt of all prior bytes (if any).
 *                           The first ACK sent by each and acknowledges the other end's initial sequence number itself,
 *                           but no data.
 */
class MeshRUDPHeader : public MeshProtocolHeader {
public:
    MeshRUDPHeader(const NetworkBufferPointer &buffer)
            : MeshProtocolHeader(buffer) {}
    MeshRUDPHeader(const NetworkBufferPointer &buffer, unsigned short offset)
            : MeshProtocolHeader(buffer, offset) {}

    /**
     * Returns a length of the packet header.
     */
    unsigned short length() const override;

    uint8_t flags() const;
    void flags(uint8_t flags);

    uint16_t windowSize() const;
    void windowSize(uint16_t windowSize);

    uint32_t seqNum() const;
    void seqNum(uint32_t seqNum);

    uint32_t ackNum() const;
    void ackNum(uint32_t ackNum);

    /**
     * RUDP flag check methods
     */
    bool isURG() const;
    bool isACK() const;
    bool isEAK() const;
    bool isRST() const;
    bool isSYN() const;
    bool isFIN() const;
};


class MeshRUDPHeaderBuilder : public PacketHeaderBuilder<MeshRUDPHeader> {
public:
    void flags(uint8_t flags);
    void windowSize(uint16_t windowSize);
    void seqNum(uint32_t seqNum);
    void ackNum(uint32_t ackNum);

    void sourcePort(uint16_t sourcePort);
    void destPort(uint16_t destPort);

    std::shared_ptr<MeshRUDPHeader> build(NetworkBufferPointer buffer, unsigned short offset) override;

private:
    uint8_t _flags;
    uint16_t _windowSize;
    uint32_t _seqNum;
    uint32_t _ackNum;

    uint16_t _sourcePort;
    uint16_t _destPort;
};


#endif //ANYFIMESH_CORE_RUDPHEADER_H
