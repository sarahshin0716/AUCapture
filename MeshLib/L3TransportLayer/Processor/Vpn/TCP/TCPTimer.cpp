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

#include "TCPTimer.h"

#include "L3TransportLayer/ControlBlock/VPN/VpnTCB.h"


uint8_t TCPTimer::tick() {
    switch (_type) {
        case TCPTimer::Type::RETRANSMISSION:
            return _tickRetransmission();
        case TCPTimer::Type::PERSISTENT:
            return _tickPersistent();
        case TCPTimer::Type::KEEP_ALIVE:
            return _tickKeepAlive();
        case TCPTimer::Type::TIME_WAIT:
            return _tickTimeWait();
        case TCPTimer::Type::DELAYED_ACK:
            return _tickDelayedACK();
    }
    return 0;
}

uint8_t TCPTimer::_tickRetransmission() {
    return 0;
}

uint8_t TCPTimer::_tickPersistent() {
    return 0;
}

uint8_t TCPTimer::_tickKeepAlive() {
    return 0;
}

uint8_t TCPTimer::_tickTimeWait() {
    return 0;
}

uint8_t TCPTimer::_tickDelayedACK() {
    if (_tcb == nullptr)
        return 0;

    if ((_tcb->flags() & TCBFlag::DELAYED_ACK) == TCBFlag::DELAYED_ACK) {
        _tcb->removeFlags(TCBFlag::DELAYED_ACK);
        //_tcb->addFlags(TCBFlag::ACKED);
        return TriggerAction::ACK_NOW;
    }

    return 0;
}
