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

#ifndef ANYFIMESH_CORE_SELECTOR_H
#define ANYFIMESH_CORE_SELECTOR_H

#include "../../Config.h"

#include <memory>
#include <set>
#include <utility>
#include <mutex>

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif


#define SELECTOR_DEFAULT_TIMEOUT -1


// Forward declarations : Link.h
class Link;

// Forward declarations
class SelectionKey;
class Selector;


enum SelectionKeyOp : unsigned short {
    /**
     * Interest set mask bit for non-action operations.
     */
    OP_NONE = 0x00,

    /**
     * Interest set mask bit for socket-accept operations.
     */
    OP_ACCEPT = 0x10,

    /**
     * Interest set mask bit for socket-connect operations.
     */
    OP_CONNECT = 0x08,

    /**
     * Interest set mask bit for read operations.
     */
    OP_READ = 0x01,

    /**
     * Interest set mask bit for write operations.
     */
    OP_WRITE = 0x04,
};


/**
 * A token representing the registration of Link with a Selector.
 *
 * A selection key is created each time a link is registered with a selector.
 *
 * Interest Set
 * The interest set determines which operation categories will be tested for readiness
 * the next one of the selector's selection method is invoked.
 * The interest set is initialized with the value given when the key is created.
 * It may be changed via interestOps(int) method.
 *
 * Ready Set
 * The ready set identifies the operation categories for which the key's link has been detected
 * to be ready by the key's selector. The ready set is initialized to zero when the key is created.
 * it may later be updated by the selector during selection operation.
 */
class SelectionKey : public std::enable_shared_from_this<SelectionKey> {
public:
    explicit SelectionKey(std::shared_ptr<Link> link, std::shared_ptr<Selector> selector)
            : _link(std::move(link)), _selector(std::move(selector)) {}

    void cancel();

    bool isAcceptable() { return isValid() && (readyOpsNoCheck() & OP_ACCEPT) == OP_ACCEPT; }
    bool isConnectable() { return isValid() && (readyOpsNoCheck() & OP_CONNECT) == OP_CONNECT; }
    bool isReadable() { return isValid() && (readyOpsNoCheck() & OP_READ) == OP_READ; }
    bool isWritable() { return isValid() && (readyOpsNoCheck() & OP_WRITE) == OP_WRITE; }

    void registerInterestOps(int ops);
    void registerInterestOpsNoCheck(int ops);
    void deregisterInterestOpsNoCheck(int ops);

    int interestOps();
    int interestOpsNoCheck();
    int readyOps();
    int readyOpsNoCheck();

    std::shared_ptr<Link> link() const { return _link; }
    std::shared_ptr<Selector> selector() const { return _selector; }

//    bool operator<(const SelectionKey &other) const;
//    bool operator==(const SelectionKey &other) const;

protected:
    friend class Selector;
    friend class WindowsSelector;
    friend class PosixSelector;

    void readyOps(int ops);

    bool isValid() const;
    void ensureValid();

private:
    int _interestOps = 0;
    int _readyOps = 0;
    bool _isValid = true;

    const std::shared_ptr<Link> _link;
    const std::shared_ptr<Selector> _selector;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(SelectionKey, ReadyOps);
    FRIEND_TEST(SelectionKey, IsAcceptable);
    FRIEND_TEST(SelectionKey, IsConnectable);
    FRIEND_TEST(SelectionKey, IsReadable);
    FRIEND_TEST(SelectionKey, IsWritable);
    FRIEND_TEST(SelectionKey, Cancel);
    FRIEND_TEST(ProxyChannel, CloseWhileConnectingSelected);
    FRIEND_TEST(ProxyChannel, CloseWhileConnectingNotSelected);
    FRIEND_TEST(ProxyChannel, CloseWhenConnected);
#endif
};


/**
 * A multiplexor of Link objects.
 *
 * A link's registration with a selector is represented by a SelectionKey object.
 * A selector maintains three of the keys method.
 * - key : The key set contains the keys representing the current link registration of this selector.
 * - selected-key : Set of keys such that each key's link was detected to be ready for at least one of the operations
 *                 identified in the key's interest set during a prior selection operation.
 * - cancelled-key : Set of keys that have been cancelled but whose links have not yet been deregistered.
 *                  This set is not directly accessible.
 */
class Selector : public std::enable_shared_from_this<Selector> {
public:
    /**
     * Opens a selector
     *
     * @return A new selector
     */
    static std::shared_ptr<Selector> open();

protected:
    /**
     * Initializer
     */
    Selector() : _isOpen(true) {}

    /**
     * Registration with a selector should be done at the Link.
     */
    friend class Link;

    /**
     * Registers the given link with this selector.
     *
     * This method is invoked by a link's registerOperation() method
     * in order to perform the actual work of registering the link with this selector.
     *
     * @param link The link to be registered
     * @param ops The initial interest set, which must be valid
     * @return A new key representing the registration of the given link with this selector.
     */
    std::shared_ptr<SelectionKey> registerOps(std::shared_ptr<Link> link, int ops);
    /**
     * Removes the given key from its link's key set.
     *
     * This method must be invoked by the selector for each link that it deregisters.
     *
     * @param key The selection key to be removed.
     */
    void deregister(std::shared_ptr<SelectionKey> key);

public:
    /**
     * Closes this selector.
     */
    void close();

    /**
     * Tells whether or not this selector is open.
     *
     * @return true if, and only if, this selector is open.
     */
    bool isOpen() const { return _isOpen; }

    /**
     * Selects a set of keys whose corresponding links are ready for I/O operations.
     *
     * This method invoke {@code select(SELECTOR_DEFAULT_TIMEOUT)}
     *
     * @return The number of selected keys.
     */
    int select() { return select(SELECTOR_DEFAULT_TIMEOUT); }
    /**
     * Selects a set of keys whose corresponding links are ready for I/O operations.
     *
     * This method performs a blocking selection operation.
     * It returns only after at least one link is selected, or timed out.
     *
     * @param timeout The maximum selection time in milliseconds.
     * @return The number of selected keys.
     */
    int select(long timeout);

    /**
     * Returns this selector's selected-key set.
     */
    std::set<std::shared_ptr<SelectionKey>> selectedKeys();

    /**
     * Cancels a selection key.
     */
    void cancel(std::shared_ptr<SelectionKey> selectionKey);

protected:
    bool _isOpen = false;

    // This mutex locks a selection process.
    std::mutex _selectionMutex;

    // The set of keys which is registered in this selector.
    std::set<std::shared_ptr<SelectionKey>> _keys;
    std::mutex _keysMutex;
    void _registerSelectionKey(std::shared_ptr<SelectionKey> key);
    void _deregisterSelectionKey(std::shared_ptr<SelectionKey> key);
    void _clearSelectionKeys();

    // Public view of keys with data ready for an operation.
    std::set<std::shared_ptr<SelectionKey>> _publicSelectedKeys;
    std::mutex _publicSelectedKeysMutex;

    // Private view of keys with data ready for an operation.
    std::set<std::shared_ptr<SelectionKey>> _privateSelectedKeys;
    std::mutex _privateSelectedKeysMutex;

    // The set of keys which is cancelled by close link.
    std::set<std::shared_ptr<SelectionKey>> _cancelledKeys;
    std::mutex _cancelledKeysMutex;
    void _deregisterCancelledKeys();

    int _maxFd = -1;
    // This method must be called from a function which locked _keysMutex.
    void _updateMaxFd();

    /**
     * Selects a set of keys whose corresponding links are ready for I/O operations.
     *
     * This method called from select(), and it returns a set of the selected keys.
     *
     * @param timeout The maximum selection time in milliseconds.
     * @return A set of the selected keys.
     */
    virtual std::set<std::shared_ptr<SelectionKey>> _doSelect(long timeout) = 0;

    virtual void wakeup() = 0;

    Anyfi::Config::mutex_type _wakeupLock;
    int wakeupIn, wakeupOut;

protected:
    Anyfi::Config::mutex_type _keysLock;
    friend class SelectionKey;
    friend class IOEngine;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(Selector, RegisterOps);
    FRIEND_TEST(Selector, DeregisterOps);
    FRIEND_TEST(Selector, MultiThreadRegister);
    FRIEND_TEST(Selector, MultiThreadDeregister);
    FRIEND_TEST(Selector, Cancel);
    FRIEND_TEST(Selector, MultiThreadCancel);
    FRIEND_TEST(Link, RegisterOperation);
    FRIEND_TEST(Link, DeregisterOperation);
    FRIEND_TEST(ProxyChannel, Connect);
    FRIEND_TEST(ProxyChannel, CloseWhileConnectingSelected);
    FRIEND_TEST(ProxyChannel, CloseWhileConnectingNotSelected);
    FRIEND_TEST(ProxyChannel, CloseWhenConnected);
#endif
};


#endif //ANYFIMESH_CORE_SELECTOR_H
