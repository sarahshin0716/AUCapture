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

#ifndef ANYFIMESH_CORE_UDPPROCESSOR_H
#define ANYFIMESH_CORE_UDPPROCESSOR_H

#include <utility>
#include <mutex>
#include "../../../../Common/Timer.h"

#include "../../../../L3TransportLayer/Processor/Mesh/MeshPacketProcessor.h"
#include "../../../../L3TransportLayer/IL3TransportLayer.h"
#include "../../../../L3TransportLayer/ControlBlock/Mesh/MeshUCB.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>

class L3TransportLayer_init_Test;
class L3TransportLayer_acceptSession_Test;
class L4AnyfiLayer_ConnectAndClose_Test;
#endif


/**
 * Max UCB count.
 * If it exceeds the max ucb count, it will be cleaned up by the LRUCache.
 */
#define MAX_MESH_UCB_COUNT 3000
/**
 * UCB expire time.
 * If ucb is not used for expire time, it will be cleaned up.
 * The ucb expire time is 5 minute.
 */
#define MESH_UCB_EXPIRE_TIME (1000 * 60 * 5)
/**
 * UCB expire timer tick period.
 */
#define MESH_UCB_EXPIRE_TIMER_PERIOD (1000 * 10)


namespace L3 {

/**
 * MeshUDPProcessor
 */
class MeshUDPProcessor : public MeshPacketProcessor {
public:
    explicit MeshUDPProcessor(std::shared_ptr<IL3ForPacketProcessor> l3, Anyfi::UUID myUid);
    ~MeshUDPProcessor();

    void connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) override;
    void read(NetworkBufferPointer buffer, Anyfi::Address src) override;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override;
    void close(ControlBlockKey key, bool force) override;

    void shutdown() override;

private:
    Anyfi::Config::mutex_type _ucbMapMutex;
    std::unique_ptr<LRUCache<ControlBlockKey, std::shared_ptr<MeshUCB>, ControlBlockKey::Hasher>> _ucbMap;
    void _addUCB(std::shared_ptr<MeshUCB> ucb, ControlBlockKey &key);
    inline void _addUCB(std::shared_ptr<MeshUCB> ucb, ControlBlockKey &&key) { _addUCB(std::move(ucb), key); }
    void _removeUCB(ControlBlockKey &key);
    inline void _removeUCB(ControlBlockKey &&key) { _removeUCB(key); }
    std::shared_ptr<MeshUCB> _findUCB(ControlBlockKey &key);
    inline std::shared_ptr<MeshUCB> _findUCB(ControlBlockKey &&key) { return _findUCB(key); }
    void _clearAllUCB();
    std::unordered_map<ControlBlockKey, std::shared_ptr<MeshUCB>, ControlBlockKey::Hasher> _copyUCBMap();

    /**
     * Timer tick method.
     * It called every 10 second.
     */
    void _timerTick();
    std::shared_ptr<Anyfi::TimerTask> _timerTask;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(MeshUDPProcessor, Close);
    FRIEND_TEST(MeshUDPProcessor, Shutdown);
    friend class ::L3TransportLayer_init_Test;
    friend class ::L3TransportLayer_acceptSession_Test;
    friend class ::L4AnyfiLayer_ConnectAndClose_Test;
#endif
};

}   // namespace L3


#endif //ANYFIMESH_CORE_UDPPROCESSOR_H
