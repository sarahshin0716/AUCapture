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

#ifndef ANYFIMESH_CORE_TCB_H
#define ANYFIMESH_CORE_TCB_H

#include "VPNControlBlock.h"
#include "../../../L3TransportLayer/ControlBlock/ControlBlock.h"
#include "../../../L3TransportLayer/Processor/Vpn/TCP/TCPTimer.h"

#include <mutex>



#define TCB_WINDOW_SIZE     65535
#define TCB_WINDOW_SCALE    0
#define TCB_MSS             7960

// Forward decalration : L3::VPNTCPProcessor
namespace L3 {
class VPNTCPProcessor;
}


/**
 * State of the TCP Control Block.
 */
enum class TCBState : unsigned short {
    /**
     * Represents no connection state at all.
     */
    CLOSED = 0,

    /**
     * Represents waiting for a connection request from any remote TCP and port.
     */
    LISTEN = 1,

    /**
     * Represents waiting for a matching connection request after having sent a connection request.
     */
    SYN_SENT = 2,

    /**
     * Represents waiting for a confirming connection request acknowledgment
     * after having both received and sent a connection request.
     */
    SYN_RECEIVED = 3,
//
// (TCBState < ESTABLISHED) are those where connections not established.
// ========================================================================================
//

    /**
     * Represents an open connection, data received can be delivered to the user.
     * The normal state for the data transfer phase of the connection.
     */
    ESTABLISHED = 4,

    /**
     * Represents waiting for a connection termination request from the local user.
     */
    CLOSE_WAIT = 5,

//
// ========================================================================================
// (TCBState > CLOSE_WAIT) are those where user has closed.
//
    /**
     * Represents waiting for a connection termination request from the remote TCP,
     * or an acknowledgment of the connection termination request previously sent.
     */
    FIN_WAIT_1 = 6,

    /**
     * Represents waiting for a connection termination request acknowledgment from the remote TCP.
     */
    CLOSING = 7,

    /**
     * Represents waiting for an acknowledgment of the connection termination request previously sent
     * to the remote TCP (which includes an acknowledgment of its connection termination request).
     */
    LAST_ACK = 8,
//
// (TCBState > CLOSE_WAIT && TCBState < FIN_WAIT_2) await ACK of FIN.
// ========================================================================================
//

    /**
     * Represents waiting for a connection termination request from the remote TCP.
     */
    FIN_WAIT_2 = 9,

    /**
     * Represents waiting for enough time to pass to be sure the remote TCP received the acknowledgment
     * of its connection termination request.
     * According to RFC-793 a connection can stay in TIME_WAIT for a maximum of four minutes known
     * as to MSL (maximum segment lifetime).
     */
    TIME_WAIT = 10,
};


/**
 * Flags of the TCB.
 */
enum TCBFlag : uint8_t {
    /**
     * Represents no flag.
     */
    NONE            = 0,

    /**
     * All packets received have been ACKed.
     */
    ACKED           = 0b00000001,

    /**
     * Waits for the DELAYED_ACK timer to do ACK.
     */
    DELAYED_ACK     = 0b00000011,

    /**
     * Should do ACK.
     */
    DO_ACK          = 0b00000111,

    /**
     * Should do FIN.
     */
    DO_FIN          = 0b00001000,
};
/**
 * If the tcb flag contains the following bit, ACK is required.
 */
#define TCB_FLAG_NEED_ACK 0b00000011


/**
 * TCP Control Block.
 */
class VpnTCB : public VPNControlBlock {
public:
    VpnTCB(ControlBlockKey &key);
    ~VpnTCB();

    /**
     * Ticks a activated TCP timers.
     *
     * @return Triggered actions.
     */
    uint8_t tickTimers();

//
// ========================================================================================
// TCB Getter/Setter
//
    inline TCBState state() const { return _state; }
    inline void state(TCBState state) { _state = state; }

    inline uint32_t seq() const { return _seq; }
    inline void seq(uint32_t seq) { _seq = seq; }
    inline void seq_random() { _seq = std::rand() % UINT32_MAX; }

    inline uint32_t ack() const { return _ack; }
    inline void ack(uint32_t ack) { _ack = ack; }

    inline uint32_t acked() const { return _acked; }
    inline void acked(uint32_t acked) { _acked = acked; }

    inline uint16_t dup_ack_count() const { return _dup_ack_count; }
    inline void dup_ack_count(uint16_t count) { _dup_ack_count = count; }
    inline void add_dup_ack_count() { _dup_ack_count++; }

    inline uint16_t sendWindow() const { return _sendWindow; }
    inline void sendWindow(uint16_t window) { _sendWindow = window; }
    inline uint16_t recvWindow() const { return _recvWindow; }
    inline void recvWindow(uint16_t window) { _recvWindow = window; }

    inline uint8_t sendWindowScale() const { return _sendWindowScale; }
    inline void sendWindowScale(uint8_t ws) { _sendWindowScale = ws; }
    inline uint8_t recvWindowScale() const { return _recvWindowScale; }
    inline void recvWindowScale(uint8_t ws) { _recvWindowScale = ws; }

    inline double calculatedSendWindow() const { return _sendWindow * pow(2, _sendWindowScale); }
    inline double calculatedRecvWindow() const { return _recvWindow * pow(2, _recvWindowScale); }

    inline uint8_t flags() const { return _flags; }
    inline void flags(uint8_t flags) { _flags = flags; }
    inline void addFlags(uint8_t flags) { _flags |= flags; }
    inline void removeFlags(uint8_t flags) { _flags &= ~flags; }

    inline uint16_t mss() const { return _mss; }
    inline void mss(uint16_t mss) { _mss = mss; }
//
// TCB Getter/Setter
// ========================================================================================
//

    /* Read Buffer methods */
    void addReadBuffer(NetworkBufferPointer buffer);

    /* Write Buffer methods */
    void addWriteBuffer(NetworkBufferPointer buffer);
    NetworkBufferPointer getWriteBuffer(unsigned int offset, unsigned short len);
    int dropWriteBuffer(uint32_t len);
    void dropUnsentWriteBuffer();
    inline size_t writeBufferCount() { return _writeBuffers.size(); }
    size_t writeBufferTotalLength();

    /**
     * Returns a size of the remain send window.
     */
    double remainSendWindow();

    /**
     * Returns a length of the un-acked bytes.
     */
    uint32_t unackedBytes();

    /**
     * Returns whether to do ACK.
     */
    inline bool doACK() { return (_flags & TCB_FLAG_NEED_ACK) == TCB_FLAG_NEED_ACK; }

private:
    TCBState _state = TCBState::CLOSED;
    uint8_t _flags = TCBFlag::NONE;

    uint32_t _seq = 0, _ack = 0;
    uint32_t _acked = 0;
    uint16_t _dup_ack_count = 0;

    uint16_t _sendWindow = 0, _recvWindow= 0;
    uint8_t _sendWindowScale = 0, _recvWindowScale = 0;

    uint16_t _mss = 0;

    // Read / Write buffer
    std::list<NetworkBufferPointer> _readBuffers;
    std::list<NetworkBufferPointer> _writeBuffers;

    TCPTimer *_timers[TCPTimer::NUM_TIMER_TYPES];
    inline void initTimer(TCPTimer::Type type) { _timers[static_cast<int>(type)] = new TCPTimer(type, this); }

protected:
    friend class L3::VPNTCPProcessor;
    Anyfi::Config::mutex_type _mutex;
};


#endif //ANYFIMESH_CORE_TCB_H
