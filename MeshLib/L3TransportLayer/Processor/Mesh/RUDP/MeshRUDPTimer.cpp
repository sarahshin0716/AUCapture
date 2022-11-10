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

#include "MeshRUDPTimer.h"

#include "../../../../L3TransportLayer/ControlBlock/Mesh/MeshRUCB.h"


uint8_t MeshRUDPTimer::tick() {
    switch (_type) {
        case MeshRUDPTimer::Type::RETRANSMISSION:
            return _tickRetransmission();
        case MeshRUDPTimer::Type::PERSISTENT:
            return _tickPersistent();
        case MeshRUDPTimer::Type::KEEP_ALIVE:
            return _tickKeepAlive();
        case MeshRUDPTimer::Type::TIME_WAIT:
            return _tickTimeWait();
        case MeshRUDPTimer::Type::DELAYED_ACK:
            return _tickDelayedACK();
    }
    return 0;
}

uint8_t MeshRUDPTimer::_tickRetransmission() {
    if (_rucb == nullptr)
        return 0;
    if (!_rucb->isREMTActivated())
        return 0;

    _rucb->rexmtAddTicks();

    if (_rucb->rexmtNumTicks() * INTERVAL_MS >= _rucb->currentRTO()) {
        return TriggerAction::RETRANSMIT;
    }

    return 0;
}

uint8_t MeshRUDPTimer::_tickPersistent() {
    return 0;
}

uint8_t MeshRUDPTimer::_tickKeepAlive() {
    return 0;
}

uint8_t MeshRUDPTimer::_tickTimeWait() {
    return 0;
}

uint8_t MeshRUDPTimer::_tickDelayedACK() {
    if (_rucb == nullptr)
        return 0;

    if ((_rucb->flags() & MeshRUCB::Flag::DELAYED_ACK) == MeshRUCB::Flag::DELAYED_ACK) {
        _rucb->removeFlags(MeshRUCB::Flag::DELAYED_ACK);
        _rucb->addFlags(MeshRUCB::Flag::ACKED);
        return TriggerAction::ACK_NOW;
    }

    return 0;
}
