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

#include "NetworkBufferPool.h"


NetworkBufferPool *NetworkBufferPool::_instance = nullptr;

NetworkBufferPool::NetworkBufferPool() {
    auto make = []()->std::shared_ptr<NetworkBuffer>
    {
        return std::make_shared<NetworkBuffer>(NETWORK_BUFFER_SIZE);
    };
    auto recycle = [](std::shared_ptr<NetworkBuffer> o)
    {
        o->reset();
    };

    _thePool = std::make_shared<resource_pool<NetworkBuffer>>(make, recycle);
}

std::shared_ptr<NetworkBuffer> NetworkBufferPool::_acquireImpl() {
    auto buf = _thePool->allocate();
    return buf;
}

void NetworkBufferPool::_releaseImpl(std::shared_ptr<NetworkBuffer> buffer) {
    assert(false);
}

void NetworkBufferPool::_clearImpl() {
    _thePool->free_unused();
}
