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

#ifndef ANYFIMESH_CORE_ANYFIBRIDGEMAP_H
#define ANYFIMESH_CORE_ANYFIBRIDGEMAP_H

#include <unordered_map>
#include <mutex>
#include <memory>
#include <type_traits>
#include <stdexcept>

#include "AnyfiBridge.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

namespace L4 {
/**
 * ControlBlockId와 AnyfiBridge를 이어주는 Map입니다. Anyfi::Config::mutex_type로 map에 대한 lock을 관리합니다.
 * @tparam T AnyfiBridge를 포함한 AnyfiBridge의 상속 클래스
 */
template<class T>
class AnyfiBridgeMap {
public:
    AnyfiBridgeMap() {
        _bridgeMap.clear();
    };

    std::shared_ptr<T> get(unsigned short cbId) {
        Anyfi::Config::lock_type guard(_bridgeMapMutex);
        return _getWithoutLock(cbId);
    }
    void addBridge(unsigned short cbId, std::shared_ptr<T> bridge) {
        Anyfi::Config::lock_type guard(_bridgeMapMutex);
        _addBridgeWithoutLock(cbId, bridge);
    }
    void removeBridge(unsigned short cbId) {
        Anyfi::Config::lock_type guard(_bridgeMapMutex);
        _removeBridgeWithoutLock(cbId);
    }
    std::vector<unsigned short> getKeys() {
        Anyfi::Config::lock_type guard(_bridgeMapMutex);
        std::vector<unsigned short> keys;
        for (auto &it: _bridgeMap) {
            keys.push_back(it.first);
        }
        return keys;
    }

    unsigned long size() {
        Anyfi::Config::lock_type guard(_bridgeMapMutex);
        return _bridgeMap.size();
    }
private:
    std::shared_ptr<T> _getWithoutLock(unsigned short cbId) {
        auto it = _bridgeMap.find(cbId);
        if (it != _bridgeMap.end())
            return it->second;
        else
            return nullptr;
    }
    void _addBridgeWithoutLock(unsigned short cbId, std::shared_ptr<T> bridge) {
        _bridgeMap[cbId] = bridge;
    }
    void _removeBridgeWithoutLock(unsigned short cbId) {
        auto it = _bridgeMap.find(cbId);
        if (it != _bridgeMap.end()) {
            _bridgeMap.erase(it);
        }
    }

    Anyfi::Config::mutex_type _bridgeMapMutex;
    std::unordered_map<unsigned short, std::shared_ptr<T>> _bridgeMap;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(AnyfiBridgeMap, init);
#endif
};
}; // namespace L4;


#endif //ANYFIMESH_CORE_ANYFIBRIDGEMAP_H
