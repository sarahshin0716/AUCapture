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

#ifndef ANYFIMESH_CORE_NETWORKBUFFERPOOL_H
#define ANYFIMESH_CORE_NETWORKBUFFERPOOL_H

#include "NetworkBuffer.h"
#include "../../../Common/resource_pool.hpp"

#include <list>
#include <mutex>
#include <memory>
// https://github.com/steinwurf/recycle


/**
 * Default NetworkBuffer size.
 */
#define NETWORK_BUFFER_SIZE 65536

/**
 * NetworkBufferPool.
 */
class NetworkBufferPool {
public:
    NetworkBufferPool();

    /**
     * Get static singleton instance.
     */
    inline static NetworkBufferPool *getInstance() {
        if (_instance == nullptr) {
            _instance = new NetworkBufferPool();
        }
        return _instance;
    }

    /**
     * Acquires a network buffer.
     */
    inline static NetworkBufferPointer acquire() {
        return getInstance()->_acquireImpl();
    }

    /**
     * Release a network buffer.
     */
    inline static void release(NetworkBufferPointer buffer) {
        getInstance()->_releaseImpl(buffer);
    }

    /**
     * Clear all buffers in pool.
     */
    inline static void clear() {
        getInstance()->_clearImpl();
    }

    /**
     * Returns the number of acquired buffers.
     */
    inline static unsigned long countAcquired() {
        auto pool = getInstance();
        return pool->_thePool->unused_resources();
    }

    /**
     * Returns the number of released buffers.
     */
    inline static unsigned long countReleased() {
        auto pool = getInstance();
        return pool->_thePool->unused_resources();
    }

protected:
    std::shared_ptr<NetworkBuffer> _acquireImpl();
    void _releaseImpl(std::shared_ptr<NetworkBuffer> buffer);
    void _clearImpl();

private:
    // Singleton instance
    static NetworkBufferPool *_instance;

    // Mutex for concurrency
    Anyfi::Config::mutex_type _poolMutex;

    std::shared_ptr<resource_pool<NetworkBuffer>> _thePool;
};


#endif //ANYFIMESH_CORE_NETWORKBUFFERPOOL_H
