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

#ifndef ANYFIMESH_CORE_MESHRUDPTIMER_H
#define ANYFIMESH_CORE_MESHRUDPTIMER_H

#include <cstdint>
#include <chrono>


class MeshRUCB;


/**
 * Mesh RUDP Timer.
 */
class MeshRUDPTimer {
public:
    /**
     * Mesh RUDP Timer mill-seconds interval.
     */
    static const unsigned short INTERVAL_MS = 100;

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

        PERSISTENT = 1,

        KEEP_ALIVE = 2,

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
         * No action required.
         */
        NO_ACTION       = 0,

        /**
         * MeshRUDPProcessor should do ACK immediately.
         * This action is triggered by Delayed ACK timer.
         */
        ACK_NOW         = 0b00000001,

        /**
         * MeshRUDPProcessor should do retransmission immediately.
         * This action is triggered by REMT(Retransmission timer).
         */
        RETRANSMIT      = 0b00000010,
    };

    /**
     * Construct with MeshRUDPTimer Type.
     */
    explicit MeshRUDPTimer(const Type _type, MeshRUCB *_rucb) : _type(_type), _rucb(_rucb) {}

    /**
     * Ticks a RUDP Timer.
     *
     * @return Triggered actions
     */
    uint8_t tick();

    const Type type() const { return _type; }

private:
    const Type _type;
    MeshRUCB *_rucb;

    uint8_t _tickRetransmission();
    uint8_t _tickPersistent();
    uint8_t _tickKeepAlive();
    uint8_t _tickTimeWait();
    uint8_t _tickDelayedACK();
};

#endif //ANYFIMESH_CORE_MESHRUDPTIMER_H
