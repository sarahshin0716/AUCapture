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

// Selector-posix.cpp should only be compiled on unix
#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
    #elif TARGET_OS_IPHONE
    #elif TARGET_OS_MAC
        #define POSIX
    #endif
#elif __linux__ || __unix__ || defined(_POSIX_VERSION)
    #define POSIX
#endif

#ifdef POSIX


#include "Selector.h"

#include <sys/select.h>
#include <utility>

#include "L1LinkLayer/Link/Link.h"
#include "L1LinkLayer/Link/AcceptServerLink.h"


int doSystemSelect(int numFds, fd_set *readFds, fd_set *writeFds, fd_set *errFds, timeval *timeout) {
    return select(numFds, readFds, writeFds, errFds, timeout);
}


class PosixSelector : public Selector {
public:
    PosixSelector();
    ~PosixSelector();

    void wakeup();

protected:
    std::set<std::shared_ptr<SelectionKey>> _doSelect(long timeout) override;

private:
    struct timeval _timeout {};
    fd_set _readSet{}, _writeSet{};
};

PosixSelector::PosixSelector()
: Selector()
{
    int pipeFds[2] = {};
    int res = pipe(pipeFds);
    if(res != 0) {
        throw std::runtime_error("cannot create wakeup pipe");
    }
    wakeupIn = pipeFds[0];
    wakeupOut = pipeFds[1];
    int flags = fcntl(wakeupIn, F_GETFL, 0);
    fcntl(wakeupIn, F_SETFL, flags | O_NONBLOCK);

    _updateMaxFd();
}

PosixSelector::~PosixSelector()
{
    ::close(wakeupIn); wakeupIn = 0;
    ::close(wakeupOut); wakeupOut = 0;
}

void PosixSelector::wakeup()
{
    char dummy[1];
    write(wakeupOut, dummy, 1);
}


std::set<std::shared_ptr<SelectionKey>> PosixSelector::_doSelect(long timeout) {
    // Lock this method to prevent this selector from select multiple times by different threads,
    // which might cause race condition.
    Anyfi::Config::lock_type guard(_selectionMutex);
    Anyfi::Config::lock_type guardKeys(_keysMutex);
    Anyfi::Config::lock_type guardSelectedKeys(_privateSelectedKeysMutex);

    // Deregister cancelledKeys, before start select-operation.
    _deregisterCancelledKeys();

    FD_ZERO(&_readSet);
    FD_ZERO(&_writeSet);

    FD_SET(wakeupIn, &_readSet);

    {
        for (const std::shared_ptr<SelectionKey> &key : _keys) {
            if (!key->isValid())
                continue;

            auto sock = key->link()->sock();
            if (sock < 0)
                continue;

            // Set the write-set
            if ((key->interestOps() & SelectionKeyOp::OP_CONNECT) == SelectionKeyOp::OP_CONNECT ||
                (key->interestOps() & SelectionKeyOp::OP_WRITE) == SelectionKeyOp::OP_WRITE)
                FD_SET(sock, &_writeSet);

            // Set the read-set
            if ((key->interestOps() & SelectionKeyOp::OP_ACCEPT) == SelectionKeyOp::OP_ACCEPT ||
                (key->interestOps() & SelectionKeyOp::OP_READ) == SelectionKeyOp::OP_READ)
                FD_SET(sock, &_readSet);
        }
    }

    if (timeout >= 0) {
        _timeout.tv_sec = timeout / 1000;
        _timeout.tv_usec = (timeout % 1000) * 1000;
        doSystemSelect(_maxFd + 1, &_readSet, &_writeSet, nullptr, &_timeout);
    } else {
        doSystemSelect(_maxFd + 1, &_readSet, &_writeSet, nullptr, NULL);
    }

    _privateSelectedKeys.clear();

    if(FD_ISSET(wakeupIn, &_readSet)) {
        char dummy[8];
        while(read(wakeupIn, dummy, 1) > 0) {

        }
    }

    for (const std::shared_ptr<SelectionKey> &key : _keys) {
        if (!key->isValid())
            continue;

        auto sock = key->link()->sock();
        if (sock < 0)
            continue;

        key->readyOps(0);

        if (FD_ISSET(sock, &_readSet)) {
            if ((key->interestOps() & SelectionKeyOp::OP_ACCEPT) == SelectionKeyOp::OP_ACCEPT)
                key->readyOps(key->readyOps() | SelectionKeyOp::OP_ACCEPT);
            else
                key->readyOps(key->readyOps() | SelectionKeyOp::OP_READ);
        }
        if (FD_ISSET(sock, &_writeSet)) {
            if (key->link()->state() == LinkState::SOCK_CONNECTING)
                key->readyOps(key->readyOps() | SelectionKeyOp::OP_CONNECT);
            else
                key->readyOps(key->readyOps() | SelectionKeyOp::OP_WRITE);
        }

        if (key->readyOps() != 0)
            _privateSelectedKeys.insert(key);
    }

    // Deregistered cancelledKeys, after privateSelectedKeys are updated.
    _deregisterCancelledKeys();

    return _privateSelectedKeys;
}


#endif