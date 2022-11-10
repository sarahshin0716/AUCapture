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

#ifndef ANYFIMESH_CORE_PACKETPROCESSOR_H
#define ANYFIMESH_CORE_PACKETPROCESSOR_H

#include "../../Common/Network/Buffer/NetworkBuffer.h"
#include "../../Common/DataStructure/LRUCache.h"
#include "../../L3TransportLayer/IL3TransportLayer.h"
#include "../../L3TransportLayer/ControlBlock/ControlBlock.h"


namespace L3 {

class IL3ForPacketProcessor;

/**
 *
 */
class PacketProcessor {
public:
    explicit PacketProcessor(std::shared_ptr<IL3ForPacketProcessor> l3) : _L3(std::move(l3)) {}

    /**
     * Connects a session to the given address
     * @param address
     * @return
     */
    virtual void connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) = 0;
    /**
     * Process the given read packet.
     */
    virtual void read(NetworkBufferPointer buffer, Anyfi::Address src) = 0;
    /**
     * Process the given write packet.
     */
    virtual void write(NetworkBufferPointer buffer, ControlBlockKey key) = 0;
    /**
     * Closes give control block
     */
    virtual void close(ControlBlockKey key, bool force) = 0;

    /**
     * Shut-down packet processor.
     */
    virtual void shutdown() = 0;

    virtual void reset() {};

protected:
    const std::shared_ptr<IL3ForPacketProcessor> _L3;
};

}   // namespace L3


#endif //ANYFIMESH_CORE_PACKETPROCESSOR_H
