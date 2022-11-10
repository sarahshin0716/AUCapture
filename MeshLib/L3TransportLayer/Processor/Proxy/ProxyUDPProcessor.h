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

#ifndef ANYFIMESH_CORE_PROXYUDPPROCESSOR_H
#define ANYFIMESH_CORE_PROXYUDPPROCESSOR_H

#include "ProxyPacketProcessor.h"
#include "../../../Common/Timer.h"
#include "../../../L3TransportLayer/ControlBlock/Proxy/ProxyUCB.h"

#include <mutex>


namespace L3 {

class ProxyUDPProcessor : public ProxyPacketProcessor {
public:
    explicit ProxyUDPProcessor(std::shared_ptr<IL3ForPacketProcessor> l3);
    ~ProxyUDPProcessor();

    void connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) override;
    void read(NetworkBufferPointer buffer, Anyfi::Address src, unsigned short linkId) override;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override;
    void close(ControlBlockKey key, bool force) override;

    void shutdown() override;

private:
    Anyfi::Config::mutex_type _ucbMapMutex;
    std::unique_ptr<LRUCache<ControlBlockKey, std::shared_ptr<ProxyUCB>, ControlBlockKey::Hasher>> _ucbMap;
    void _addUCB(const std::shared_ptr<ProxyUCB> &ucb, ControlBlockKey &key);
    inline void _addUCB(const std::shared_ptr<ProxyUCB> &ucb, ControlBlockKey &&key) { _addUCB(ucb, key); }
    void _removeUCB(ControlBlockKey &key);
    inline void _removeUCB(ControlBlockKey &&key) { _removeUCB(key); }
    std::shared_ptr<ProxyUCB> _findUCB(ControlBlockKey &key);
    inline std::shared_ptr<ProxyUCB> _findUCB(ControlBlockKey &&key) { return _findUCB(key); }
    std::unordered_map<ControlBlockKey, std::shared_ptr<ProxyUCB>, ControlBlockKey::Hasher> _copyUCBMap();

    /**
     * Timer tick method.
     * It called every 10 second.
     */
    void _timerTick();
    std::shared_ptr<Anyfi::TimerTask> _timerTask;
};

}   // namespace L3


#endif //ANYFIMESH_CORE_PROXYUDPPROCESSOR_H
