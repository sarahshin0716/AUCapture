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

#include "MeshRUDPProcessor.h"

#include "Log/Frontend/Log.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"

#include <algorithm>


NetworkBufferPointer
_addRUDPHeader(NetworkBufferPointer buffer, uint8_t flags, uint16_t window,
               uint16_t srcPort, uint16_t dstPort, uint32_t seq, uint32_t ack) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    MeshRUDPHeaderBuilder builder;
    builder.flags(flags);
    builder.windowSize(window);
    builder.sourcePort(srcPort);
    builder.destPort(dstPort);
    builder.seqNum(seq);
    builder.ackNum(ack);
    builder.build(buffer, buffer->getWritePos());

    return buffer;
}


L3::MeshRUDPProcessor::MeshRUDPProcessor(std::shared_ptr<L3::IL3ForPacketProcessor> l3, Anyfi::UUID myUid)
        : MeshPacketProcessor(std::move(l3), myUid) {
    // Set timer called every 100 ms.
    _timerTask = Anyfi::Timer::schedule(
            std::bind(&L3::MeshRUDPProcessor::_timerTick, this),
            100, MeshRUDPTimer::INTERVAL_MS);
}

L3::MeshRUDPProcessor::~MeshRUDPProcessor() {
    shutdown();
}

void
L3::MeshRUDPProcessor::connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) {
    if (address.type() != Anyfi::AddrType::MESH_UID)
        throw std::invalid_argument("Destination address type should be MESH_UID");
    if (address.connectionProtocol() != Anyfi::ConnectionProtocol::MeshRUDP)
        throw std::invalid_argument("Connection protocol must be MeshUDP");

    Anyfi::Address src;
    src.setMeshUID(_myUid);
    src.port(_L3->assignPort());
    src.connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);

    ControlBlockKey key(src, address);
    auto rucb = std::make_shared<MeshRUCB>(key);
    rucb->state(MeshRUCB::State::ESTABLISHED);
    rucb->seq(1);
    rucb->ack(1);
    rucb->mss(MESH_RUDP_SEGMENT_SIZE);
    rucb->sendWindow(MESH_RUDP_DEFAULT_WINDOW);
    rucb->sendWindowScale(MESH_RUDP_DEFAULT_WS);
    rucb->recvWindow(MESH_RUDP_DEFAULT_WINDOW);
    rucb->recvWindowScale(MESH_RUDP_DEFAULT_WS);
    _addRUCB(rucb, key);

    // Send SYN packet
    auto synBuf = NetworkBufferPool::acquire();
    synBuf->setBackwardMode();
    _addRUDPHeader(synBuf, MeshRUDPFlag::SYN, MESH_RUDP_DEFAULT_WINDOW,
                   src.port(), address.port(), rucb->seq(), rucb->ack());
    _L3->writeFromPacketProcessor(address, synBuf);

    rucb->seq(rucb->seq() + 1);
    rucb->acked(rucb->seq());

    if (callback != nullptr)
        callback(rucb);
}

void L3::MeshRUDPProcessor::read(NetworkBufferPointer buffer, Anyfi::Address src) {
    auto packet = std::make_shared<MeshRUDPPacket>(buffer, buffer->getReadPos());
    auto header = packet->header();

    Anyfi::Address dst;
    dst.setMeshUID(_myUid);
    dst.port(header->destPort());
    dst.connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);

    src.port(header->sourcePort());
    src.connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);

    ControlBlockKey key(dst, src);
    auto rucb = _findRUCB(key);
    if (rucb == nullptr) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_MESH_RUDP, "AnonymousSession",
                          "(" +
                          to_string(packet->header()->seqNum()) + ", " +
                          to_string(packet->header()->ackNum()) + ") " +
                          to_string(packet->payloadSize()));

        if (header->isSYN()) {
            rucb = _processSYN(header.get(), key);
            _addRUCB(rucb, key);
            _L3->onSessionConnected(rucb);
        } else {
            // Send 0 ACK
            auto ackBuf = NetworkBufferPool::acquire();
            ackBuf->setBackwardMode();
            _addRUDPHeader(ackBuf, MeshRUDPFlag::ACK, 0, dst.port(), src.port(), 0, 0);
            _L3->writeFromPacketProcessor(src, ackBuf);
        }
    } else {
        ProcessResult result(NONE, nullptr);
        {
            // Locks the MeshRUCB before process a packet with the MeshRUCB.
            Anyfi::Config::lock_type guard(rucb->_mutex);

            if (header->isSYN()) result = _processDuplicateSYN(header.get(), rucb);
            else if (header->isRST()) result = _processRST(header.get(), rucb);
            else if (header->isFIN()) result = _processFIN(packet.get(), rucb);
            else if (header->isACK()) result = _processACK(packet.get(), rucb);
        }

        // Pass read buffer
        if (result.second != nullptr)
            _L3->onReadFromPacketProcessor(rucb, result.second);

        // Notify connection state change
        switch (result.first) {
            case CONNECTED: {
                _L3->onSessionConnected(rucb);
                break;
            }
            case CLOSED: {
                _L3->onSessionClosed(rucb, false);
                break;
            }
            case RESET: {
                _L3->onSessionClosed(rucb, true);
                break;
            }
            default:
                break;
        }
    }
}

void L3::MeshRUDPProcessor::write(NetworkBufferPointer buffer, ControlBlockKey key) {
    auto rucb = _findRUCB(key);
    if (rucb == nullptr) {
        Anyfi::Log::error("L3", "RUCB is nullptr, MeshRUDPProcessor#write");
        return;
    }

    Anyfi::Config::lock_type rucbGaurd(rucb->_mutex);
    rucb->addWriteBuffer(buffer);
    if (rucb->remainSendWindow() > 0)
        _doACK(rucb);
}

void L3::MeshRUDPProcessor::close(ControlBlockKey key, bool force) {
    auto rucb = _findRUCB(key);
    if (rucb == nullptr) {
        std::cerr << "MeshRUCB is nullptr, MeshRUDP#close" << std::endl;
        return;
    }

    Anyfi::Config::lock_type guard(rucb->_mutex);
    _closeImplWithoutLock(rucb, force);
}

void L3::MeshRUDPProcessor::shutdown() {
    if (_timerTask != nullptr) {
        Anyfi::Timer::cancel(_timerTask);
        _timerTask = nullptr;
    }
}

void L3::MeshRUDPProcessor::_addRUCB(const std::shared_ptr<MeshRUCB> &rucb, ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_rucbMapMutex);
        _rucbMap[key] = rucb;
    }

    _L3->onControlBlockCreated(rucb);
}

void L3::MeshRUDPProcessor::_removeRUCB(ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_rucbMapMutex);
        _rucbMap.erase(key);
    }

    _L3->onControlBlockDestroyed(key);
}

std::shared_ptr<MeshRUCB> L3::MeshRUDPProcessor::_findRUCB(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_rucbMapMutex);

    for (auto &item : _rucbMap) {
        if (item.second->key() == key)
            return item.second;
    }

    return nullptr;
}

void L3::MeshRUDPProcessor::_timerTick() {
    _rucbMapMutex.lock();
    auto rucbMap = _rucbMap;
    _rucbMapMutex.unlock();

    for (const auto &rucb : rucbMap) {
        // Locks the RUCB, before tick and perform triggered action.
        Anyfi::Config::lock_type guard(rucb.second->_mutex);

        uint8_t triggeredActions = rucb.second->tickTimers();
        if (triggeredActions == 0)
            continue;

        if ((triggeredActions & MeshRUDPTimer::TriggerAction::ACK_NOW) == MeshRUDPTimer::TriggerAction::ACK_NOW) {
            _doACK(rucb.second);
        }
        if ((triggeredActions & MeshRUDPTimer::TriggerAction::RETRANSMIT) == MeshRUDPTimer::TriggerAction::RETRANSMIT) {
            _retransmitAll(rucb.second);
        }
    }
}

std::shared_ptr<MeshRUCB> L3::MeshRUDPProcessor::_processSYN(MeshRUDPHeader *header, ControlBlockKey &key) {
    auto rucb = std::make_shared<MeshRUCB>(key);
    rucb->state(MeshRUCB::State::ESTABLISHED);
    rucb->seq(1);
    rucb->ack(header->seqNum() + 1);
    rucb->acked(1);
    rucb->mss(MESH_RUDP_SEGMENT_SIZE);
    rucb->sendWindow(MESH_RUDP_DEFAULT_WINDOW);
    rucb->sendWindowScale(MESH_RUDP_DEFAULT_WS);
    rucb->recvWindow(MESH_RUDP_DEFAULT_WINDOW);
    rucb->recvWindowScale(MESH_RUDP_DEFAULT_WS);

    return rucb;
}

L3::MeshRUDPProcessor::ProcessResult
L3::MeshRUDPProcessor::_processDuplicateSYN(MeshRUDPHeader *header, std::shared_ptr<MeshRUCB> rucb) {
    return ProcessResult(NONE, nullptr);
}

L3::MeshRUDPProcessor::ProcessResult
L3::MeshRUDPProcessor::_processRST(MeshRUDPHeader *header, std::shared_ptr<MeshRUCB> rucb) {
    return ProcessResult(NONE, nullptr);
}

L3::MeshRUDPProcessor::ProcessResult
L3::MeshRUDPProcessor::_processFIN(MeshRUDPPacket *packet, std::shared_ptr<MeshRUCB> rucb) {
    return ProcessResult(NONE, nullptr);
}

L3::MeshRUDPProcessor::ProcessResult
L3::MeshRUDPProcessor::_processACK(MeshRUDPPacket *packet, std::shared_ptr<MeshRUCB> rucb) {
    if (rucb == nullptr) {
        Anyfi::Log::error(__func__, "rucb is null");
        return ProcessResult(NONE, nullptr);
    }

    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_MESH_RUDP, __func__,
                      to_string(rucb->id()) + ", (" +
                      to_string(packet->header()->seqNum()) + ", " +
                      to_string(packet->header()->ackNum()) + ") " +
                      to_string(packet->payloadSize()));

    // When SYN retransmission required
    if (packet->header()->seqNum() == 0 && packet->header()->ackNum() == 0) {
        _retransmit(rucb, 1);
        return ProcessResult(NONE, nullptr);
    }

    // Drop invalid order packet.
    if (!_validateOrdering(packet->header().get(), rucb))
        return ProcessResult(NONE, nullptr);

    if (rucb->state() == MeshRUCB::State::LAST_ACK) {
        // close RUDP Session
        rucb->state(MeshRUCB::State::CLOSED);
        _removeRUCB(rucb->key());

        return ProcessResult(CLOSED, nullptr);
    }

    // Check MeshRUCB is established
    if (rucb->state() != MeshRUCB::State::ESTABLISHED) {
        Anyfi::Log::error(__func__, "rucb is not established");
        return ProcessResult(NONE, nullptr);
    }

    // Check duplicated ACK
    if (packet->payloadSize() == 0 && rucb->acked() == packet->header()->ackNum())
        rucb->add_dup_ack_count();
    else rucb->dup_ack_count(0);
    if (rucb->dup_ack_count() >= 3) {
        // Fast retransmit
        _retransmitAll(rucb);
//        _retransmit(rucb, packet->header()->ackNum());
    }

    auto newAckedBytes = packet->header()->ackNum() - rucb->acked();
    if (newAckedBytes > 0) {
        rucb->dropWriteBuffer(newAckedBytes);

        // Deactivate retransmission timer
        if (rucb->rexmtSeq() <= packet->header()->ackNum())
            rucb->deactivateREMT();
    }

    // Update MeshRUCB
    rucb->acked(packet->header()->ackNum());
    rucb->sendWindow(packet->header()->windowSize());

    NetworkBufferPointer readBuffer;
    if (packet->payloadSize() != 0) {
        // When contains payload
        readBuffer = _readPayload(packet, rucb);

        if (rucb->doACK()) _doACK(rucb);
        else rucb->addFlags(MeshRUCB::Flag::DELAYED_ACK);
    } else {
        // Empty ACK, move send window
        if ((newAckedBytes > 0 && rucb->writeBufferTotalLength() > 0) || rucb->doACK())
            _doACK(rucb);
    }

    return ProcessResult(NONE, readBuffer);
}

void L3::MeshRUDPProcessor::_closeImplWithoutLock(std::shared_ptr<MeshRUCB> rucb, bool force) {

}

NetworkBufferPointer
L3::MeshRUDPProcessor::_readPayload(MeshRUDPPacket *packet, std::shared_ptr<MeshRUCB> rucb) {
    auto payloadBuf = packet->buffer();
    payloadBuf->setReadPos(packet->payloadPos());
    rucb->ack(packet->header()->seqNum() + packet->payloadSize());
    return payloadBuf;
}

bool L3::MeshRUDPProcessor::_validateOrdering(MeshRUDPHeader *header, std::shared_ptr<MeshRUCB> rucb) {
    if (header->seqNum() == rucb->ack()) {
        // Valid : Receive in-order packet
        return true;
    } else if (header->seqNum() > rucb->ack()) {
        Anyfi::Log::warn(__func__, "Receive out-of-order packet : Send Duplicate ACK");
        _doACK(rucb);
    } else if (header->seqNum() < rucb->ack()) {
//        Anyfi::Log::warn(__func__, "Spurious Retransmission seq(" + to_string(header->seqNum()) + ") rucb ack(" +
//                                   to_string(rucb->ack()));
        _doACK(rucb);
    }
    return false;
}

void L3::MeshRUDPProcessor::_doACK(std::shared_ptr<MeshRUCB> rucb) {
    auto offset = rucb->unackedBytes();
    auto buffer = rucb->getWriteBuffer(offset,
                                       (unsigned short) (rucb->mss() < rucb->remainSendWindow()
                                                         ? rucb->mss() : rucb->remainSendWindow()));
    if (buffer == nullptr) {
        // Sends an empty ACK
        buffer = NetworkBufferPool::acquire();
        buffer->setBackwardMode();

        _addRUDPHeader(buffer, MeshRUDPFlag::ACK, rucb->recvWindow(),
                       rucb->key().srcAddress().port(), rucb->key().dstAddress().port(),
                       rucb->seq(), rucb->ack());
        _L3->writeFromPacketProcessor(rucb->key().dstAddress(), buffer);

        rucb->flags(MeshRUCB::Flag::ACKED);
        return;
    }

    unsigned int payloadLength = 0;
    while (buffer != nullptr) {
        payloadLength = buffer->getWritePos() - buffer->getReadPos();
        if (payloadLength == 0)
            break;

        /**
         * Sends a packet with payload
         */
        _addRUDPHeader(buffer, MeshRUDPFlag::ACK, rucb->recvWindow(),
                       rucb->key().srcAddress().port(), rucb->key().dstAddress().port(),
                       rucb->seq(), rucb->ack());
        _L3->writeFromPacketProcessor(rucb->key().dstAddress(), buffer);

        rucb->seq(rucb->seq() + payloadLength);
        rucb->flags(MeshRUCB::Flag::ACKED);
        rucb->activateREMT();

        offset += payloadLength;
        buffer = rucb->getWriteBuffer(offset,
                                      (unsigned short) (rucb->mss() < rucb->remainSendWindow()
                                                        ? rucb->mss() : rucb->remainSendWindow()));
    }
}

void L3::MeshRUDPProcessor::_retransmit(std::shared_ptr<MeshRUCB> rucb, uint32_t seq) {
    NetworkBufferPointer buffer;

    if (seq == 1) {
        // SYN retransmission
        // The SYN packet doesn't have a payload.
        buffer = NetworkBufferPool::acquire();
        buffer->setBackwardMode();
    } else {
        // ACK retransmission
        // The ACK retransmission packet should contain a payload.
        auto offset = seq - rucb->acked();
        buffer = rucb->getWriteBuffer(offset, rucb->mss());
        if (buffer == nullptr)
            return;
    }

    Anyfi::Log::error(__func__, to_string(rucb->id()) + ", " + to_string(seq));

    _addRUDPHeader(buffer,
                   seq == 1 ? MeshRUDPFlag::SYN : MeshRUDPFlag::ACK,
                   rucb->recvWindow(),
                   rucb->key().srcAddress().port(), rucb->key().dstAddress().port(),
                   seq, rucb->ack());
    _L3->writeFromPacketProcessor(rucb->key().dstAddress(), buffer);

    rucb->rexmtNumTicks(0);
    rucb->rexmtMoveShift();
}

void L3::MeshRUDPProcessor::_retransmitAll(std::shared_ptr<MeshRUCB> rucb) {
    auto total = rucb->sendWindow();
    auto seq = rucb->acked();
    for (auto bytesRext = 0; bytesRext < total;) {
        auto len = std::min((uint16_t) rucb->mss(), (uint16_t) (total - bytesRext));
        auto buffer = rucb->getWriteBuffer(seq - rucb->acked(), len);
        if (buffer == nullptr)
            return;

        auto payloadLen = buffer->getWritePos() - buffer->getReadPos();
        if (payloadLen == 0)
            return;

        _addRUDPHeader(buffer, MeshRUDPFlag::ACK, rucb->recvWindow(),
                       rucb->key().srcAddress().port(), rucb->key().dstAddress().port(),
                       seq, rucb->ack());
        _L3->writeFromPacketProcessor(rucb->key().dstAddress(), buffer);

        bytesRext += payloadLen;
        seq += payloadLen;
    }

    rucb->rexmtNumTicks(0);
    rucb->rexmtMoveShift();
}
