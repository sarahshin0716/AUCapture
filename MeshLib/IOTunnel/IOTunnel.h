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

#ifndef ANYFIMESH_CORE_IOTUNNEL_H
#define ANYFIMESH_CORE_IOTUNNEL_H

#include <memory>

#include "../Common/Network/Buffer/NetworkBuffer.h"


class IOTunnel {
public:
    static std::shared_ptr<IOTunnel> open();
    virtual ~IOTunnel() = default;

    /**
     * Starts VPN I/O.
     */
    virtual bool startVPN() = 0;
    /**
     * Stops a VPN.
     */
    virtual void stopVPN() = 0;
    /**
     * Reads a buffer from the VPN.
     */
    virtual NetworkBufferPointer readVPN() = 0;
    /**
     * Writes a given buffer to the VPN.
     */
    virtual void writeVPN(NetworkBufferPointer buffer) = 0;

private:
    static std::shared_ptr<IOTunnel> _instance;
};


#endif //ANYFIMESH_CORE_IOTUNNEL_H
