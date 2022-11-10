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

#ifndef ANYFIMESH_CORE_IOENGINE_H
#define ANYFIMESH_CORE_IOENGINE_H

#include <thread>

#include "../../Common/Concurrency/ThreadPool.h"
#include "../../L1LinkLayer/Link/Link.h"
#include "../../L1LinkLayer/Link/WifiP2PLink.h"
#include "Selector.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif


#define IO_THREAD_COUNT 4


// Forward declarations : this
class IOEngine;
class ConcreteIOEngine;


/**
 * Engine for executing {@link Link} operations.
 *
 * The L2LinkLayer registers I/O operations with the Link to the IOEngine,
 * which commits the operations on the behalf of the Link when it is read to do so.
 *
 * The IOEngine uses a Selector to support the I/O multiplexing.
 */
class IOEngine {
public:
    /**
     * Starts the IOEngine.
     * If the IOEngine has already started, ignore it.
     *
     * @return The shared_ptr of the started IOEngine.
     */
    static std::shared_ptr<IOEngine> start(std::shared_ptr<Selector> selector);
    IOEngine(const IOEngine &other) = delete;
    IOEngine &operator=(const IOEngine &other) = delete;

private:
    friend class ConcreteIOEngine;
    IOEngine(std::shared_ptr<Selector> selector);
    ~IOEngine();
    static std::shared_ptr<ConcreteIOEngine> _instance;

public:
    /**
     * Shuts down the IOEngine.
     */
    void shutdown();

    typedef std::function<void(std::shared_ptr<Link> fromLink, std::shared_ptr<Link> acceptedLink)> AcceptSockCallback;
    typedef std::function<void(std::shared_ptr<Link> p2pLink)> AcceptP2PCallback;
    typedef std::function<void(std::shared_ptr<Link> link, NetworkBufferPointer buffer)> ReadCallback;

    bool isOpen() const { return _isOpen; }

private:
    bool _isOpen = false;

    // Selector relative entities
    const std::shared_ptr<Selector> _selector;
    std::thread _selectorThread;
    void _selectorThreadImpl(std::shared_ptr<Selector> selector);

    ThreadPool *_ioThreadPool;

    inline void _performAccept(std::shared_ptr<Link> link);
    inline void _performConnect(std::shared_ptr<Link> link);
    inline void _performWrite(std::shared_ptr<Link> link);
    inline void _performRead(std::shared_ptr<Link> link);

    void notifySockAccept(std::shared_ptr<Link> fromLink, std::shared_ptr<Link> acceptedLink);

//    AcceptP2PCallback _acceptP2PCallback;
//    void notifyP2PAccept(std::shared_ptr<Link> p2pLink);

    void notifyRead(std::shared_ptr<Link> link, NetworkBufferPointer buffer);

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(IOEngine, Shutdown);
#endif
};


#endif //ANYFIMESH_CORE_IOENGINE_H
