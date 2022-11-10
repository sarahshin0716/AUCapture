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

#ifndef ANYFIMESH_CORE_VPNPACKETPROCESSOR_H
#define ANYFIMESH_CORE_VPNPACKETPROCESSOR_H

#include <utility>

#include "../../../L3TransportLayer/Processor/PacketProcessor.h"
#include "../../../L3TransportLayer/Packet/TCPIP_L2_Internet/IPPacket.h"


namespace L3 {

/**
 * Base class of VPN Packet Processor.
 */
class VPNPacketProcessor : public PacketProcessor {
public:
    VPNPacketProcessor(std::shared_ptr<IL3ForPacketProcessor> l3) : PacketProcessor(std::move(l3)) {}

    /**
     * Input IPPacket to VPN Packet Processor.
     */
    virtual void input(std::shared_ptr<IPPacket> packet) = 0;

    void write(NetworkBufferPointer buffer, ControlBlockKey key) override = 0;
    void close(ControlBlockKey key, bool force) override = 0;

    void shutdown() override = 0;

/**
 * Not allowed in VPNPacketProcessor
 * Do not override this methods.
 */
private:
    void connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) override {
        throw std::runtime_error("connect() method not allowed in VPNPacketProcessor");
    };
    void read(NetworkBufferPointer buffer, Anyfi::Address src) override {
        throw std::runtime_error("read() method not allowed in VPNPacketProcessor");
    };

};

}   // namespace L3


#endif //ANYFIMESH_CORE_VPNPACKETPROCESSOR_H
