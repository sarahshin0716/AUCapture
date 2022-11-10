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

#ifndef ANYFIMESH_CORE_RUDPPROCESSOR_H
#define ANYFIMESH_CORE_RUDPPROCESSOR_H

#include "../../../../L3TransportLayer/ControlBlock/Mesh/MeshRUCB.h"
#include "../../../../L3TransportLayer/Processor/Mesh/MeshPacketProcessor.h"
#include "../../../../L3TransportLayer/Packet/TCPIP_L4_Application/MeshRUDPHeader.h"
#include "../../../../L3TransportLayer/Packet/TCPIP_L4_Application/MeshRUDPPacket.h"

#include "../../../../Common/Timer.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>

class L4AnyfiLayer_ConnectAndClose_Test;
#endif


#define MESH_RUDP_SEGMENT_SIZE 1460
#define MESH_RUDP_DEFAULT_WINDOW 8000
#define MESH_RUDP_DEFAULT_WS 2


namespace L3 {

class MeshRUDPProcessor : public MeshPacketProcessor {
public:
    MeshRUDPProcessor(std::shared_ptr<IL3ForPacketProcessor> l3, Anyfi::UUID myUid);
    ~MeshRUDPProcessor();

    typedef std::function<void(std::shared_ptr<ControlBlock> cb)> ConnectCallback;
    void connect(Anyfi::Address address, ConnectCallback callback) override;
    void read(NetworkBufferPointer buffer, Anyfi::Address src) override;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override;
    void close(ControlBlockKey key, bool force) override;

    void shutdown() override;

private:
    Anyfi::Config::mutex_type _rucbMapMutex;
    std::unordered_map<ControlBlockKey, std::shared_ptr<MeshRUCB>, ControlBlockKey::Hasher> _rucbMap;
    inline void _addRUCB(const std::shared_ptr<MeshRUCB> &rucb, ControlBlockKey &key);
    inline void _addRUCB(const std::shared_ptr<MeshRUCB> &rucb, ControlBlockKey &&key) { _addRUCB(rucb, key); }
    inline void _removeRUCB(ControlBlockKey &key);
    inline void _removeRUCB(ControlBlockKey &&key) { _removeRUCB(key); }
    inline std::shared_ptr<MeshRUCB> _findRUCB(ControlBlockKey &key);
    inline std::shared_ptr<MeshRUCB> _findRUCB(ControlBlockKey &&key) { return _findRUCB(key); }

    /**
     * Mesh RUDP timer tick method.
     * It called every 100 ms.
     */
    void _timerTick();
    std::shared_ptr<Anyfi::TimerTask> _timerTask;

    enum ConnectionStateChange : uint8_t {
        NONE = 0,
        CONNECTED = 1,
        CLOSED = 2,
        RESET = 3,
    };
    typedef std::pair<ConnectionStateChange, NetworkBufferPointer> ProcessResult;
    std::shared_ptr<MeshRUCB> _processSYN(MeshRUDPHeader *header, ControlBlockKey &key);
    ProcessResult _processDuplicateSYN(MeshRUDPHeader *header, std::shared_ptr<MeshRUCB> rucb);
    ProcessResult _processRST(MeshRUDPHeader *header, std::shared_ptr<MeshRUCB> rucb);
    ProcessResult _processFIN(MeshRUDPPacket *packet, std::shared_ptr<MeshRUCB> rucb);
    ProcessResult _processACK(MeshRUDPPacket *packet, std::shared_ptr<MeshRUCB> rucb);

    /**
     * Close function implementation without lock.
     * Call this function after lock the given RUCB.
     */
    void _closeImplWithoutLock(std::shared_ptr<MeshRUCB> rucb, bool force);

    /**
     * Reads a payload in the rudp packet.
     * It updates RUDP, and pass payload to the L3.
     */
    NetworkBufferPointer _readPayload(MeshRUDPPacket *packet, std::shared_ptr<MeshRUCB> rucb);

    /**
     * Check packet ordering is valid.
     * If receive out-of-order packet, sends a duplicate ack.
     * If receive duplicate ack packet, do retransmit.
     *
     * @return true if valid, false otherwise.
     */
    bool _validateOrdering(MeshRUDPHeader *header, std::shared_ptr<MeshRUCB> rucb);

    /**
     * Sends an ACK to the packets received so far.
     */
    void _doACK(std::shared_ptr<MeshRUCB> rucb);

    /**
     * Sends a EACK
     */
    void _doEACK(std::shared_ptr<MeshRUCB> rucb);

    /**
     * Retransmit a "seq" sequence packet.
     */
    void _retransmit(std::shared_ptr<MeshRUCB> rucb, uint32_t seq);
    /**
     * Retransmit all packets as much as send window size.
     */
    void _retransmitAll(std::shared_ptr<MeshRUCB> rucb);

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    friend class ::L4AnyfiLayer_ConnectAndClose_Test;
#endif
};

}   // namespace L3


#endif //ANYFIMESH_CORE_RUDPPROCESSOR_H
