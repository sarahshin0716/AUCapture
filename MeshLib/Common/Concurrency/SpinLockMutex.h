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

#ifndef ANYFIMESH_CORE_SPINLOCKMUTEX_H
#define ANYFIMESH_CORE_SPINLOCKMUTEX_H

#include <atomic>


class SpinLockMutex {
public:
    SpinLockMutex() : _lock(ATOMIC_FLAG_INIT) {}

    void lock() {
        while (_lock.test_and_set(std::memory_order_acquire));
    }

    void unlock() {
        _lock.clear(std::memory_order_release);
    }

private:
    std::atomic_flag _lock;
};

#endif //ANYFIMESH_CORE_SPINLOCKMUTEX_H
