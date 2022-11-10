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

#ifndef ANYFIMESH_CORE_RUCB_H
#define ANYFIMESH_CORE_RUCB_H

#include "MeshControlBlock.h"
#include "MeshRUDPTimer.h"

#include <mutex>


// Forward decalration : L3::VPNTCPProcessor
namespace L3 {
class MeshRUDPProcessor;
}


/**
 * Mesh Reliable UDP Control Block.
 */
class MeshRUCB : public MeshControlBlock {
public:
    enum class State : unsigned short {
        /**
         * Represents no connection state at all.
         */
        CLOSED = 0,
//
// (RUCBState < ESTABLISHED) are those where connections not established.
// ========================================================================================
//

        /**
         * Represents an open connection, data received can be delivered to L4.
         * The normal state for the data transfer phase of the connection.
         */
        ESTABLISHED = 4,

        /**
         * Represents waiting for a connection termination request from the L4.
         */
        CLOSE_WAIT = 5,

//
// ========================================================================================
// (RUCBState > CLOSE_WAIT) are those where user has closed.
//
        /**
         * Represents waiting for a connection termination request from the remote Mesh RUDP,
         * or an acknowledgment of the connection termination request previously sent.
         */
        FIN_WAIT_1 = 6,

        /**
         * Represents waiting for a connection termination request acknowledgment from the remote Mesh RUDP.
         */
        CLOSING = 7,

        /**
         * Represents waiting for an acknowledgment of the connection termination request previously sent
         * to the remote Mesh RUDP (which includes an acknowledgment of its connection termination request).
         */
        LAST_ACK = 8,
//
// (RUCBState > CLOSE_WAIT && TCBState < FIN_WAIT_2) await ACK of FIN.
// ========================================================================================
//

        /**
         * Represents waiting for a connection termination request from the remote TCP.
         */
        FIN_WAIT_2 = 9,

        /**
         * Represents waiting for enough time to tpass to be sure the remote Mesh RUDP received the acknowledgment
         * of its connection termination request.
         * According to to (TCP)RFC-793 a connection can stay in TIME_WAIT for a maximum of for minutes known
         * as to MSL (maximum segment lifetime).
         */
        TIME_WAIT = 10,
    };

    /**
     * Flags of the RUCB.
     */
    enum Flag : uint8_t {
        /**
         * Represents no flag.
         */
        NONE                = 0,

        /**
         * All packet received have been ACKed.
         */
        ACKED               = 0b00000001,

        /**
         * Waits for the DELAYED_ACK timer to do ACK.
         */
        DELAYED_ACK         = 0b00000011,

        /**
         * Should do ACK.
         */
        DO_ACK              = 0b00000111,

        /**
         * Retransmission timer is activated
         */
        ACTIVATE_REMT       = 0B00001000,
    };
    /**
     * If the tcb flag contains the following bit, ACK is required.
     */
    static const uint8_t FLAG_NEED_ACK = 0b00000011;


    MeshRUCB(ControlBlockKey &key);
    ~MeshRUCB();

    /**
     * Ticks a activated RUCB timers.
     *
     * @return Triggered actions.
     */
    uint8_t tickTimers();


//
// ========================================================================================
// RUCB Getter/Setter
//
    inline State state() const { return _state; }
    inline void state(State state) { _state = state; }

    inline uint8_t flags() const { return _flags; }
    inline void flags(uint8_t flags) { _flags = flags; }
    inline void addFlags(uint8_t flags) { _flags |= flags; }
    inline void removeFlags(uint8_t flags) { _flags &= ~flags; }

    inline uint32_t seq() const { return _seq; }
    inline void seq(uint32_t seq) { _seq = seq; }
    inline uint32_t ack() const { return _ack; }
    inline void ack(uint32_t ack) { _ack = ack; }

    inline uint32_t acked() const { return _acked; }
    inline void acked(uint32_t acked) { _acked = acked; }

    inline uint16_t dup_ack_count() const { return _dup_ack_count; }
    inline void dup_ack_count(uint16_t count) { _dup_ack_count = count; }
    inline void add_dup_ack_count() { _dup_ack_count++; }

    inline uint16_t mss() const { return _mss; }
    inline void mss(uint16_t mss) { _mss = mss; }

    inline uint16_t sendWindow() const { return _sendWindow; }
    inline void sendWindow(uint16_t window) { _sendWindow = window; }
    inline uint16_t recvWindow() const { return _recvWindow; }
    inline void recvWindow(uint16_t window) { _recvWindow = window; }

    inline uint8_t sendWindowScale() const { return _sendWindowScale; }
    inline void sendWindowScale(uint8_t ws) { _sendWindowScale = ws; }
    inline uint8_t recvWindowScale() const { return _recvWindowScale; }
    inline void recvWindowScale(uint8_t ws) { _recvWindowScale = ws; }

    inline uint32_t calculatedSendWindow() const { return static_cast<uint32_t>(_sendWindow * pow(2, _sendWindowScale)); }
    inline uint32_t calculatedRecvWindow() const { return static_cast<uint32_t>(_recvWindow * pow(2, _recvWindowScale)); }

    inline uint16_t srtt() const { return _srtt; }
    inline void srtt(uint16_t srtt) { _srtt = srtt; }
    inline uint16_t rttvar() const { return _rttvar; }
    inline void rttvar(uint16_t rttvar) { _rttvar = rttvar; }

    inline uint16_t rexmtNumTicks() const { return _rexmtNumTicks; }
    inline void rexmtNumTicks(uint16_t ticks) { _rexmtNumTicks = ticks; }
    inline void rexmtAddTicks() { _rexmtNumTicks++; }
    inline uint32_t rexmtSeq() const { return _rexmtSeq; }
    inline void rexmtMoveShift() { if (_rexmtShift < REXMT_SHIFT_MAX) _rexmtShift++; }
    inline void rexmtClear() { _rexmtNumTicks = 0; _rexmtSeq = 0; _rexmtShift = 0; }
//
// RUCB Getter/Setter
// ========================================================================================
//

    void addWriteBuffer(NetworkBufferPointer buffer);
    NetworkBufferPointer getWriteBuffer(unsigned int offset, unsigned short len);
    void dropWriteBuffer(unsigned short len);
    inline size_t writeBufferCount() { return _writeBuffers.size(); }
    size_t writeBufferTotalLength();

    /**
     * Activate retransmission timer
     */
    void activateREMT();
    /**
     * Deactivate retransmission timer
     */
    void deactivateREMT();

    unsigned int currentRTO() const;

    /**
     * Returns a size of the remain send window.
     */
    uint32_t remainSendWindow();

    /**
     * Returns a length of the un-acked bytes.
     */
    uint32_t unackedBytes();

    /**
     * Returns whether to do ACK.
     */
    inline bool doACK() { return (_flags & FLAG_NEED_ACK) == FLAG_NEED_ACK; }
    /**
     * Returns whether retransmission timer is activated
     */
    inline bool isREMTActivated() { return (_flags & Flag::ACTIVATE_REMT) != 0; }

private:
    State _state = State::CLOSED;
    uint8_t _flags = Flag::NONE;

    uint32_t _seq = 0, _ack = 0;
    uint32_t _acked = 0;
    uint16_t _dup_ack_count = 0;

    uint16_t _mss = 0;

    /**
     * Window variables
     */
    uint16_t _sendWindow = 0, _recvWindow = 0;
    uint8_t _sendWindowScale = 0, _recvWindowScale = 0;

    /**
     * Transmit timing stuff.
     */
    uint16_t _srtt = 0;
    uint16_t _rttvar = 0;

    /**
     * Retransmission timer variables.
     */
    uint16_t _rexmtNumTicks = 0;
    uint32_t _rexmtSeq = 0;
    uint8_t _rexmtShift = 0;
    const uint8_t REXMT_SHIFT_MAX = 12;
    const uint8_t REXMT_BACKOFF[13] = {1, 1, 1, 2, 2, 4, 8, 16, 16, 16, 16, 16, 16};

    std::list<NetworkBufferPointer> _writeBuffers;

    MeshRUDPTimer *_timers[MeshRUDPTimer::NUM_TIMER_TYPES];
    inline void initTimer(MeshRUDPTimer::Type type) { _timers[static_cast<int>(type)] = new MeshRUDPTimer(type, this); }

protected:
    friend class L3::MeshRUDPProcessor;
    Anyfi::Config::mutex_type _mutex;
};


#endif //ANYFIMESH_CORE_RUCB_H
