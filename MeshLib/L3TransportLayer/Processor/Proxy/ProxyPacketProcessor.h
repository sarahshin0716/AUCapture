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

#ifndef ANYFIMESH_CORE_PROXYPACKETPROCESSOR_H
#define ANYFIMESH_CORE_PROXYPACKETPROCESSOR_H

#include "../../../L3TransportLayer/Processor/PacketProcessor.h"


#define PROXY_SESSION_LOCAL_ADDR "127.0.0.1"


/**
 * Max UCB count.
 * If it exceeds the max ucb count, it will be cleaned up by the LRUCache.
 */
#define MAX_PROXY_UCB_COUNT 100
/**
 * Max TCB count.
 * If it exceeds the max tcb count, it will be cleaned up by the LRUCache.
 */
#define MAX_PROXY_TCB_COUNT 600
/**
 * Proxy Control Block expire time.
 * If Proxy Control Block is not used for expire time, it will be cleaned up.
 * The Proxy Control Block expire time is 5 minute.
 */
#define PROXY_EXPIRE_TIME (1000 * 60 * 5)

/**
 * Proxy Control Block expire timer tick period.
 */
#define PROXY_EXPIRE_TIMER_PERIOD (1000 * 10)

namespace L3 {

class ProxyPacketProcessor : public PacketProcessor {
public:
    explicit ProxyPacketProcessor(std::shared_ptr<IL3ForPacketProcessor> l3)
            : PacketProcessor(std::move(l3)) {}

    void connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) override = 0;
    virtual void read(NetworkBufferPointer buffer, Anyfi::Address src, unsigned short linkId) = 0;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override = 0;
    void close(ControlBlockKey key, bool force) override = 0;

    void shutdown() override = 0;

    static Anyfi::Address srcAddr(unsigned short channelId) {
        Anyfi::Address addr;
        addr.connectionType(Anyfi::ConnectionType::Proxy);
        addr.type(Anyfi::AddrType::IPv4);
        addr.addr(PROXY_SESSION_LOCAL_ADDR);
        addr.port(channelId);
        return addr;
    }

private:
    void read(NetworkBufferPointer buffer, Anyfi::Address src) override {
        throw std::runtime_error("Deprecated method");
    }
};

}


#endif //ANYFIMESH_CORE_PROXYPACKETPROCESSOR_H
