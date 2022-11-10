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

#ifndef ANYFIMESH_CORE_MESHPACKETPROCESSOR_H
#define ANYFIMESH_CORE_MESHPACKETPROCESSOR_H

#include "../../../L3TransportLayer/Processor/PacketProcessor.h"

#include <utility>


namespace L3 {

class MeshPacketProcessor : public PacketProcessor {
public:
    explicit MeshPacketProcessor(std::shared_ptr<IL3ForPacketProcessor> l3, Anyfi::UUID myUid);

    void connect(Anyfi::Address address, std::function<void(std::shared_ptr<ControlBlock> cb)> callback) override = 0;
    void read(NetworkBufferPointer buffer, Anyfi::Address src) override = 0;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override = 0;
    void close(ControlBlockKey key, bool force) override = 0;

    void shutdown() override = 0;
protected:
    Anyfi::UUID _myUid;
};

}


#endif //ANYFIMESH_CORE_MESHPACKETPROCESSOR_H
