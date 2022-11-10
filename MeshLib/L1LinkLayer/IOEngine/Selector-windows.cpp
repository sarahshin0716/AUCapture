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

// Selector-windows.cpp should only be compiled on windows
#ifdef _WIN32


#include "Selector.h"

#include <WinSock2.h>

#include "L1LinkLayer/Link/Link.h"
#include "L1LinkLayer/Link/AcceptServerLink.h"


int doSystemSelect(int numFds, fd_set *readFds, fd_set *writeFds, fd_set *errFds, timeval *timeout) {
    return select(numFds, readFds, writeFds, errFds, timeout);
}


class WindowsSelector : public Selector {
protected:
    std::set<std::shared_ptr<SelectionKey>> _doSelect(long timeout) override;

private:
    TIMEVAL _timeout;
    fd_set _readSet, _writeSet;
};


std::set<std::shared_ptr<SelectionKey>> WindowsSelector::_doSelect(long timeout) {
    // Lock this method to prevent this selector from select multiple times by different threads,
    // which might cause race condition.
    Anyfi::Config::lock_type guard(_selectionMutex);

    // Deregister cancelledKeys, before start select-operation.
    _deregisterCancelledKeys();

    FD_ZERO(&_readSet);
    FD_ZERO(&_writeSet);

    // Lock underlying operation to prevent race condition with register operation.
    _keysMutex.lock();
    for (const std::shared_ptr<SelectionKey> &key : _keys) {
        // Locks a selection key
        Anyfi::Config::lock_type keyGuard(key->_cancelMutex);

        if (!key->isValid() || key->link()->sock() < 0)
            continue;

        // Set the write-set
        if ((key->interestOps() & SelectionKeyOp::OP_CONNECT) == SelectionKeyOp::OP_CONNECT ||
            (key->interestOps() & SelectionKeyOp::OP_WRITE) == SelectionKeyOp::OP_WRITE)
            FD_SET(key->link()->sock(), &_writeSet);

        // Set the read-set
        if ((key->interestOps() & SelectionKeyOp::OP_ACCEPT) == SelectionKeyOp::OP_ACCEPT ||
            (key->interestOps() & SelectionKeyOp::OP_READ) == SelectionKeyOp::OP_READ)
            FD_SET(key->link()->sock(), &_readSet);
    }
    _keysMutex.unlock();

    _timeout.tv_sec = timeout / 1000;
    _timeout.tv_usec = (timeout % 1000) * 1000;

    // Lock and clear private selectedKeys, before do actual-select operation.
    _privateSelectedKeysMutex.lock();
    _privateSelectedKeys.clear();
    _privateSelectedKeysMutex.unlock();


    doSystemSelect(_maxFd + 1, &_readSet, &_writeSet, nullptr, &_timeout);

    // Lock underlying operation to prevent race condition with register operation.
    _keysMutex.lock();
    _privateSelectedKeysMutex.lock();
    for (const std::shared_ptr<SelectionKey> &key : _keys) {
        // Locks a selection key
        Anyfi::Config::lock_type keyGuard(key->_cancelMutex);

        if (!key->isValid() || key->link()->sock() < 0)
            continue;

        key->readyOps(0);

        if (FD_ISSET(key->link()->sock(), &_readSet)) {
            if ((key->interestOps() & SelectionKeyOp::OP_ACCEPT) == SelectionKeyOp::OP_ACCEPT)
                key->readyOps(key->readyOps() | SelectionKeyOp::OP_ACCEPT);
            else key->readyOps(key->readyOps() | SelectionKeyOp::OP_READ);
        }
        if (FD_ISSET(key->link()->sock(), &_writeSet)) {
            if (key->link()->state() == LinkState::SOCK_CONNECTING)
                key->readyOps(key->readyOps() | SelectionKeyOp::OP_CONNECT);
            else key->readyOps(key->readyOps() | SelectionKeyOp::OP_WRITE);
        }

        if (key->readyOps() != 0)
            _privateSelectedKeys.insert(key);
    }
    _keysMutex.unlock();
    _privateSelectedKeysMutex.unlock();

    // Deregistered cancelledKeys, after privateSelectedKeys are updated.
    _deregisterCancelledKeys();

    return _privateSelectedKeys;
}


#endif