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

#ifndef ANYFIMESH_CORE_LRUCACHE_H
#define ANYFIMESH_CORE_LRUCACHE_H

#include <utility>
#include <list>
#include <functional>
#include <unordered_map>

#include "../../Log/Frontend/Log.h"


/**
 * LRU(Least Recently Used) cache based on hashmap and linkedlist.
 *
 * @tparam K A type of the key.
 * @tparam V A type of the value.
 * @tparam H A type of the key hasher.
 */
template<typename K, typename V, typename H = std::hash<K>>
class LRUCache {
public:
    typedef typename std::pair<K, V> KeyValuePairType;
    typedef typename std::list<KeyValuePairType>::iterator ListIteratorType;
    typedef typename std::function<void(K, V)> CleanUpCallbackType;

    /**
     * Creates a LRUCache with a maximum size.
     *
     * @param maxSize Maximum LRUCache sizes.
     */
    explicit LRUCache(size_t maxSize) : _maxSize(maxSize) {}

    /**
     * Caches value for key.
     * The value is moved to the head of the queue.
     *
     * @param key
     * @param value
     */
    void put(const K &key, const V &value) {
        auto it = _cacheMap.find(key);
        _cacheList.push_front(KeyValuePairType(key, value));
        if (it != _cacheMap.end()) {
            _cacheList.erase(it->second);
            _cacheMap.erase(it);
        }
        _cacheMap[key] = _cacheList.begin();

        if (_cacheMap.size() > _maxSize) {
            auto last = _cacheList.end();
            last--;
            _cacheMap.erase(last->first);
            _cacheList.pop_back();

            // Notify cleaned-up
            if (_cleanUpCallback != nullptr) {
                _cleanUpCallback(last->first, last->second);
            }
        }
    }

    /**
     * Returns the value for key if it exists in the cache.
     * If value was returned, it is moved to the head of the queue.
     * This returns null if a value is not cached.
     *
     * @param key
     * @return value The value for the key.
     */
    const V &get(const K &key) {
        auto it = _cacheMap.find(key);
        if (it == _cacheMap.end()) {
            throw std::range_error("There is no such key in cache");
        } else {
            _cacheList.splice(_cacheList.begin(), _cacheList, it->second);
            return it->second->second;
        }
    }

    /**
     * Removes a matching cache for key.
     *
     * @return true if remove sucess, false otherwise.
     */
    bool remove(const K &key) {
        auto it = _cacheMap.find(key);
        if (it != _cacheMap.end()) {
            _cacheList.erase(it->second);
            _cacheMap.erase(it);
            return true;
        }
        return false;
    }

    /**
     * Clear all caches.
     */
    void clear() {
        _cacheList.clear();
        _cacheMap.clear();
    }

    /**
     * Returns whether key is exists in the cache.
     *
     * @param key
     * @return true if, and only if, key is exists in the cache.
     */
    bool exists(const K &key) const {
        return _cacheMap.find(key) != _cacheMap.end();
    }

    /**
     * Returns the cache size.
     */
    size_t size() const { return _cacheMap.size(); }

    /**
     * Returns the maximum cache size.
     */
    size_t maxSize() const { return _maxSize; }

    /**
     * Returns a iterator(begin) of the cache list.
     */
    ListIteratorType listIterBegin() { return _cacheList.begin(); }

    /**
     * Returns a iterator(end) of the cache list.
     */
    ListIteratorType listIterEnd() { return _cacheList.end(); }

    /**
     * Attach a given clean-up callback.
     */
    void attachCleanUpCallback(const CleanUpCallbackType &cleanUpCallback) { _cleanUpCallback = cleanUpCallback; }

private:
    std::list<KeyValuePairType> _cacheList;
    std::unordered_map<K, ListIteratorType, H> _cacheMap;
    size_t _maxSize;

    CleanUpCallbackType _cleanUpCallback;
};

#endif //ANYFIMESH_CORE_LRUCACHE_H
