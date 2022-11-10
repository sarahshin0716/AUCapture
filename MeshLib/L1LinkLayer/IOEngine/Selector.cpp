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

#include "Selector.h"


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

#ifdef _WIN32
    #include "Selector-windows.cpp"
#elif defined(POSIX)
    #include "Selector-posix.cpp"
#endif


#include <memory>

#include "../../L1LinkLayer/Link/Link.h"


//
// ========================================================================================
// SelectionKey codes
//
void SelectionKey::cancel() {
    if (_isValid) {
        _selector->cancel(shared_from_this());
        _isValid = false;
    }
}

void SelectionKey::registerInterestOps(int ops) {
    ensureValid();
    if ((ops & ~_link->validOps()) != 0)
        throw std::invalid_argument("Invalid Ops");

    std::lock_guard<std::mutex> guard(_selector->_keysLock);
    _interestOps = _interestOps | ops;
}

void SelectionKey::registerInterestOpsNoCheck(int ops)
{
    if ((ops & ~_link->validOps()) != 0)
        throw std::invalid_argument("Invalid Ops");

    std::lock_guard<std::mutex> guard(_selector->_keysLock);
    _interestOps = _interestOps | ops;
}

void SelectionKey::deregisterInterestOpsNoCheck(int ops)
{
    if ((ops & ~_link->validOps()) != 0)
        throw std::invalid_argument("Invalid Ops");

    std::lock_guard<std::mutex> guard(_selector->_keysLock);
    if ((_interestOps & ops) == ops)
        _interestOps = _interestOps ^ ops;
}


int SelectionKey::interestOps() {
    ensureValid();

    std::lock_guard<std::mutex> guard(_selector->_keysLock);
    return _interestOps;
}

int SelectionKey::interestOpsNoCheck()
{
    std::lock_guard<std::mutex> guard(_selector->_keysLock);
    return _interestOps;
}

//bool SelectionKey::operator<(const SelectionKey &other) const {
//    return _link->id() < other.link()->id();
//}
//
//bool SelectionKey::operator==(const SelectionKey &other) const {
//    return _link->id() == other.link()->id();
//}

void SelectionKey::readyOps(int ops) {
    if ((ops & ~_link->validOps()) != 0)
        throw std::invalid_argument("Invalid Ops");

    _readyOps = ops;
}

int SelectionKey::readyOps() {
    ensureValid();
    return _readyOps;
}

int SelectionKey::readyOpsNoCheck() {
    return _readyOps;
}

bool SelectionKey::isValid() const {
    return _isValid;
}

void SelectionKey::ensureValid() {
    if (!_isValid)
        throw std::runtime_error("Invalid SelectionKey");
}
//
// SelectionKey codes
// ========================================================================================
//


//
// ========================================================================================
// Selector codes
//
std::shared_ptr<Selector> Selector::open() {
    std::shared_ptr<Selector> sharedPtr;
#ifdef _WIN32
    sharedPtr.reset(new WindowsSelector());
#elif (__APPLE__ && TARGET_OS_MAC) || __linux__ || __unix__ || defined(_POSIX_VERSION)
    sharedPtr.reset(new PosixSelector());
#endif
    return sharedPtr;
}

void Selector::close() {
    Anyfi::Config::lock_type guardWakeup(_wakeupLock);

    wakeup();

    Anyfi::Config::lock_type guard(_selectionMutex);
    Anyfi::Config::lock_type guardKeys(_keysMutex);
    Anyfi::Config::lock_type guardSelectedKeys(_privateSelectedKeysMutex);

    _isOpen = false;

    _deregisterCancelledKeys();
}

std::shared_ptr<SelectionKey> Selector::registerOps(std::shared_ptr<Link> link, int ops) {
    std::shared_ptr<SelectionKey> key = std::make_shared<SelectionKey>(link, shared_from_this());
    key->registerInterestOpsNoCheck(ops);

    _registerSelectionKey(key);

    return key;
}

void Selector::deregister(std::shared_ptr<SelectionKey> key) {
    _deregisterSelectionKey(std::move(key));
}

int Selector::select(long timeout) {
    std::set<std::shared_ptr<SelectionKey>> selectedKeys = _doSelect(timeout);

    // Lock underlying operation to prevent race condition with selectedKeys operation.
    _publicSelectedKeysMutex.lock();
    _publicSelectedKeys = selectedKeys;
    _publicSelectedKeysMutex.unlock();

    return static_cast<int>(selectedKeys.size());
}

std::set<std::shared_ptr<SelectionKey>> Selector::selectedKeys() {
    // Lock this method to avoid race condition with select().
    std::lock_guard<std::mutex> guard(_publicSelectedKeysMutex);

    return _publicSelectedKeys;
}

void Selector::cancel(std::shared_ptr<SelectionKey> selectionKey) {
    Anyfi::Config::lock_type guardWakeup(_wakeupLock);

    wakeup();

    // Lock this method to avoid race condition with cancelledKeys().
    std::lock_guard<std::mutex> guard(_selectionMutex);
    std::lock_guard<std::mutex> guardCancelledKeys(_cancelledKeysMutex);

    _cancelledKeys.insert(selectionKey);
}

void Selector::_registerSelectionKey(std::shared_ptr<SelectionKey> key) {
    // Lock this method to avoid race condition at _keys
    std::lock_guard<std::mutex> guard(_keysMutex);

    _keys.insert(key);

    _updateMaxFd();
}

void Selector::_deregisterSelectionKey(std::shared_ptr<SelectionKey> key) {

    _keys.erase(key);
    _privateSelectedKeys.erase(key);

    _updateMaxFd();
}

void Selector::_clearSelectionKeys() {
    // Lock this method to avoid race condition at _keys
    std::lock_guard<std::mutex> guard(_keysMutex);

    _keys.clear();

    _updateMaxFd();
}

void Selector::_deregisterCancelledKeys() {
    // Lock this method to avoid race condition with _cancelledKeys.
    std::lock_guard<std::mutex> guard(_cancelledKeysMutex);

    for (auto iter = _cancelledKeys.begin(); iter != _cancelledKeys.end();) {
        _deregisterSelectionKey(*iter);
        iter = _cancelledKeys.erase(iter);
    }
}


void Selector::_updateMaxFd() {
    int max = -1;
    for (const std::shared_ptr<SelectionKey> &key : _keys) {
        if (key->link()->sock() > max)
            max = key->link()->sock();
    }
    if(wakeupIn > max)
        max = wakeupIn;

    _maxFd = max;
}

//
// Selector codes
// ========================================================================================
//
