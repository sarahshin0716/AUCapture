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

#include <iostream>
#include <iomanip>

#include "../Common/Network/Buffer/NetworkBufferPool.h"
#include "Uuid.h"
#include "Hash/md5.h"

Anyfi::UUID generateUuidFromMacAddress(Anyfi::UUID::PlatformID platform, const std::string &macAddress);

Anyfi::UUID generateUuidWithPlatformAndID(Anyfi::UUID::PlatformID platform, const char *id);

Anyfi::UUID::UUID() {
    memset(_value, 0, sizeof(_value));
}

Anyfi::UUID::UUID(const char *uuid) {
    if (uuid) {
        memcpy(_value, uuid, sizeof(_value));
    } else {
        memset(_value, 0, sizeof(_value));
    }
}

Anyfi::UUID::UUID(NetworkBufferPointer buffer) {
    buffer->getBytes(&_value[0], UUID_BYTES);
}

Anyfi::UUID::UUID(NetworkBufferPointer buffer, uint32_t offset) {
    buffer->getBytes(&_value[0], UUID_BYTES, offset);
}

Anyfi::UUID Anyfi::UUID::generateUuidFromiOS(char *iosUUID) {
    return generateUuidWithPlatformAndID(UUID::PlatformID::iOS, iosUUID);
}

Anyfi::UUID Anyfi::UUID::generateUuidFromAndroid(const std::string &macAddress) {
    return generateUuidFromMacAddress(UUID::PlatformID::Android, macAddress);
}

Anyfi::UUID Anyfi::UUID::generateUuidFromWindows(const std::string &macAddress) {
    return generateUuidFromMacAddress(UUID::PlatformID::Windows, macAddress);
}

Anyfi::UUID Anyfi::UUID::generateUuidFromLinux(const std::string &macAddress) {
    return generateUuidFromMacAddress(UUID::PlatformID::Linux, macAddress);
}

Anyfi::UUID Anyfi::UUID::generateUuidFromOsX(const std::string &macAddress) {
    return generateUuidFromMacAddress(UUID::PlatformID::OsX, macAddress);
}

//NetworkBufferPointer Anyfi::UUID::toPacket() {
//    NetworkBufferPointer buffer = NetworkBufferPool::acquire();
//    toPacket(buffer);
//    return buffer;
//}

void Anyfi::UUID::toPacket(NetworkBufferPointer buffer) {
    if (buffer->isBackwardMode()) {
        for (int i = UUID_BYTES - 1; i >= 0; --i) {
            auto data = _value[i];
            buffer->put<uint8_t>(data);
        }
    } else {
        for (int i = 0; i < UUID_BYTES; ++i) {
            auto data = _value[i];
            buffer->put<uint8_t>(data);
        }
    }
}

std::string Anyfi::UUID::toString() const {
    std::stringstream ss;
    ss << std::hex;
    for (const auto &v : _value) {
        ss << (int) v;
    }
    return ss.str();
}

bool Anyfi::UUID::operator==(const UUID &other) const {
    for (int i = 0; i < UUID_BYTES; ++i)
        if (_value[i] != other._value[i]) return false;
    return true;
}

bool Anyfi::UUID::operator!=(const UUID &other) const {
    for (int i = 0; i < UUID_BYTES; ++i)
        if (_value[i] != other._value[i]) return true;
    return false;
}

Anyfi::UUID Anyfi::UUID::operator+(const UUID &other) const {
    std::string result(UUID_BYTES, 0);
    bool over = false;
    for (int i = UUID_BYTES - 1; i >= 0; i--) {
        short sum = _value[i] + other._value[i];
        if (over) {
            sum += 1;
            over = false;
        }
        if (sum > CHAR_MAX) {
            over = true;
            sum -= CHAR_MAX;
        }
        result[i] = (char) sum;
    }
    return Anyfi::UUID(result.c_str());
}

bool Anyfi::UUID::operator<(const UUID &other) const {
    for (int i = 0; i < UUID_BYTES; ++i) {
        if (_value[i] == other._value[i])
            continue;
        else
            return _value[i] < other._value[i];
    }
    return false;
}

bool Anyfi::UUID::operator>(const UUID &other) const {
    return other < *this;
}

size_t Anyfi::UUID::Hasher::operator()(const Anyfi::UUID &uuid) const {
    size_t result = 0;
    const size_t prime = 31;
    for (size_t i = 0; i < UUID_BYTES; ++i) {
        result = uuid._value[i] + (result * prime);
    }
    return result;
}

bool Anyfi::UUID::Comparator::operator()(const UUID &left, const UUID &right) const {
    return left == right;
}

Anyfi::UUID generateUuidFromMacAddress(Anyfi::UUID::PlatformID platform, const std::string &macAddress) {
    return generateUuidWithPlatformAndID(platform, md5(macAddress).c_str());
}

Anyfi::UUID generateUuidWithPlatformAndID(Anyfi::UUID::PlatformID platform, const char *id) {
    auto *temp = new char[UUID_BYTES];
    for (int i = 0; i < UUID_BYTES - 1; ++i) {
        temp[i + 1] = id[i];
    }
    temp[0] = platform;

    auto uuid = Anyfi::UUID(temp);
    delete[] temp;

    return uuid;
}
