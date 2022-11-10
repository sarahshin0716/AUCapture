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

#ifndef ANYFIMESH_CORE_ENDIAN_H
#define ANYFIMESH_CORE_ENDIAN_H

#include <cstdint>
#include <algorithm>


namespace Endian {

bool isBigEndian();

template<typename T>
T swap(T &value) {
    auto &raw = reinterpret_cast<uint8_t &>(value);

    T ret;
    auto &retRef = reinterpret_cast<uint8_t &>(ret);
    std::reverse_copy(&raw, &raw + sizeof(T), &retRef);

    return ret;
}

template<typename T>
inline T hostToNetworkOrder(T &value) {
    if (!isBigEndian())
        return swap<T>(value);
    return value;
}

template<typename T>
inline T hostToNetworkOrder(T &&value) {
	return hostToNetworkOrder<T>(value);
}

template<typename T>
inline T networkToHostOrder(T &value) {
    if (!isBigEndian())
        return swap<T>(value);
    return value;
}

template<typename T>
inline T networkToHostOrder(T &&value) {
	return networkToHostOrder<T>(value);
}

}   // Endian namespace

#endif //ANYFIMESH_CORE_ENDIAN_H
