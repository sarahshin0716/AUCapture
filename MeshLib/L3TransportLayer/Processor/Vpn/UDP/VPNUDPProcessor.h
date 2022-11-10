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

#ifndef ANYFIMESH_CORE_VPNUDPPROCESSOR_H
#define ANYFIMESH_CORE_VPNUDPPROCESSOR_H

#include <utility>

#include "../../../../Common/Timer.h"
#include "../../../../L3TransportLayer/Processor/Vpn/VPNPacketProcessor.h"
#include "../../../../L3TransportLayer/ControlBlock/VPN/VpnUCB.h"


/**
 * Max UCB count.
 * If it exceeds the max ucb count, it will be cleaned up by the LRUCache.
 */
#define MAX_VPN_UCB_COUNT 100
/**
 * UCB expire time.
 * If ucb is not used for expire time, it will be cleaned up.
 * The ucb expire time is 5 minute.
 */
#define VPN_UCB_EXPIRE_TIME (1000 * 60 * 5)
/**
 * UCB expire timer tick period.
 */
#define VPN_UCB_EXPIRE_TIMER_PERIOD (1000 * 10)

/* Actual DNS Server: 8.8.8.8 */
#define VPN_DNS_SERVER_IPV4 0x8080808
/* Private DNS Tunnel: 10.0.0.2 */
#define VPN_DNS_TUNNEL_IPV4 0xa000002
/* Private Network: 10.0.0.1 */
#define VPN_ADDRESS_IPV4 0xa000001

namespace L3 {

class VPNUDPProcessor : public VPNPacketProcessor {
public:
    explicit VPNUDPProcessor(std::shared_ptr<IL3ForPacketProcessor> l3);
    ~VPNUDPProcessor();

    void input(std::shared_ptr<IPPacket> packet) override;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override;
    void close(ControlBlockKey key, bool force) override;

    void shutdown() override;
    void clear();

private:
    Anyfi::Config::mutex_type _ucbMapMutex;
    std::unique_ptr<LRUCache<ControlBlockKey, std::shared_ptr<VpnUCB>, ControlBlockKey::Hasher>> _ucbMap;
    void _addUCB(std::shared_ptr<VpnUCB> ucb, ControlBlockKey &key);
    inline void _addUCB(std::shared_ptr<VpnUCB> ucb, ControlBlockKey &&key) { _addUCB(std::move(ucb), key); }
    void _removeUCB(ControlBlockKey &key);
    inline void _removeUCB(ControlBlockKey &&key) { _removeUCB(key); }
    std::shared_ptr<VpnUCB> _findUCB(ControlBlockKey &key);
    inline std::shared_ptr<VpnUCB> _findUCB(ControlBlockKey &&key) { return _findUCB(key); }
    std::unordered_map<ControlBlockKey, std::shared_ptr<VpnUCB>, ControlBlockKey::Hasher> _copyUCBMap();

    /**
     * Timer tick method.
     * It called every 10 second.
     */
    void _timerTick();
    std::shared_ptr<Anyfi::TimerTask> _timerTask;
};

}   // namespace L3


#endif //ANYFIMESH_CORE_VPNUDPPROCESSOR_H
