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

#ifndef ANYFIMESH_CORE_TCPTIMER_H
#define ANYFIMESH_CORE_TCPTIMER_H

#include <chrono>


// Forward declaration : VpnTCB
class VpnTCB;


/**
 * TCP Timer
 */
class TCPTimer {
public:
    static const unsigned char NUM_TIMER_TYPES = 5;
    enum Type : unsigned char {
        /**
         * The retransmission timer manages retransmission timeouts (RTOs), which occur when a preset interval between
         * the sending of a datagram and the returning acknowledgement is exceeded.
         * The value of the timeout tends to vary, depending on the network type, to compensate for speed differences.
         * If the timer expires, the datagram is retransmitted with an adjusted RTO, which is usually increased
         * exponentially to a maximum preset limit. If the maximum limit is exceeded, connection failure is assumed
         * and error messages are passed back to the upper-layer application.
         *
         * Values for the timeout are determined by measuring the average time data takes to be transmitted
         * to another machine and the acknowledgment received back, which is called the round trip time, or RTT.
         * From experiments, these RTTs are averaged by a formula that develops an expected value,
         * called the smoothed round trip time, or SRTT. This value is then increased to account for unforeseen delays.
         */
        RETRANSMISSION = 0,

        /**
         * The persistence timer handles a fairly rare occurrence.
         * It is conceivable that a receive window will have a value of 0,
         * causing the sending machine to pause transmission.
         * The message to restart sending may be lost, causing an infinite delay.
         * The persistence timer waits a preset time and then sends a 1-byte segment at predetermined intervals
         * to ensure that the receiving machine is still clogged.
         *
         * The receiving machine will resend the zero window size message after receiving one of these status segments,
         * if it is still backlogged.
         * If the window is open, a message giving the new value is returned and communications resumed.
         */
        PERSISTENT = 1,

        /**
         * When a connection has been idle for a long time, the keepalive timer may go off to cause one side
         * to check whether the other side is still there.
         * If it fails to respond, the connection is terminated.
         * This feature is controversial because it adds overhead and may terminate an otherwise healthy connection
         * due to a transient network partition.
         */
        KEEP_ALIVE = 2,

        /**
         * The timewait timer used on each TCP connection is the one used in the TIME WAIT state while closing.
         * It runs for twice the maximum packet lifetime to make sure that when a connection is closed.
         * all packets created by it have died off.
         */
        TIME_WAIT = 3,

        /**
         * The delayed ack timer manages a trigger of ACK.
         *
         * Data received on a TCP socket has to be acknowledged, either by piggybacking the acknowledgment (ACK)
         * on an outbound data message, or by sending a stand-alone ACK if no outbound data exists for a period of time.
         *
         * The ACK should not be excessively delayed, the delay must be less than 0.1 second.
         */
        DELAYED_ACK = 4,
    };

    enum TriggerAction : uint8_t {
        /**
         * No action is required.
         */
        NO_ACTION = 0b00000000,

        /**
         * TCPProcessor should do ACK immediately.
         * This action is triggered by Delayed ACK Timer.
         */
        ACK_NOW = 0b00000001,
    };

    /**
     * Construct with TCPTimer type.
     */
    explicit TCPTimer(const Type _type, VpnTCB *tcb) : _type(_type), _tcb(tcb) {}

    /**
     * Ticks a TCP Timer.
     *
     * @return Triggered actions.
     */
    uint8_t tick();

    const Type type() const { return _type; }

private:
    const Type _type;
    VpnTCB *_tcb;

    uint8_t _tickRetransmission();
    uint8_t _tickPersistent();
    uint8_t _tickKeepAlive();
    uint8_t _tickTimeWait();
    uint8_t _tickDelayedACK();
};


#endif //ANYFIMESH_CORE_TCPTIMER_H
