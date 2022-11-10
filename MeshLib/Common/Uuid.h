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

#ifndef ANYFIMESH_CORE_UUID_H
#define ANYFIMESH_CORE_UUID_H

#include <cstdint>
#include <string>
#include <functional>

#include "../Common/Network/Buffer/NetworkBuffer.h"

#define UUID_BYTES 16


namespace Anyfi {

/**
 * Anyfi's Unique User Identification value.
 *
 * The UUID is 128-bit(16 bytes) values.
 * First 8-bits(1 byte) represents a platform identification.
 * Next 120-bits(15 bytes) represents a UUID payload.
 */
class UUID {
public:
    explicit UUID();
    explicit UUID(const char* uuid);
    explicit UUID(NetworkBufferPointer buffer);
    explicit UUID(NetworkBufferPointer buffer, uint32_t offset);
    /**
     * Generates a UUID from the iOS.
     */
    static UUID generateUuidFromiOS(char* iosUUID);
    /**
     * Generates a UUID from the Android.
     */
    static UUID generateUuidFromAndroid(const std::string &macAddress);
    /**
     * Generates a UUID from the Windows.
     */
    static UUID generateUuidFromWindows(const std::string &macAddress);
    /**
     * Generates a UUID from the Linux.
     */
    static UUID generateUuidFromLinux(const std::string &macAddress);
    /**
     * Generates a UUID from the OS X.
     */
    static UUID generateUuidFromOsX(const std::string &macAddress);

    bool operator==(const UUID &other) const;
    bool operator!=(const UUID &other) const;
    UUID operator+(const UUID &other) const;
    bool operator<(const UUID &other) const;
    bool operator>(const UUID &other) const;
    struct Hasher {
        size_t operator()(const Anyfi::UUID &uuid) const;
    };

    struct Comparator {
        bool operator()(const UUID &left, const UUID &right) const;
    };

    /**
     * NetworkBuffer 형태로 변환
     * @return UUID의 NetworkBuffer
     */
//    NetworkBufferPointer toPacket();
    void toPacket(NetworkBufferPointer buffer);

    std::string toString() const;

    /**
     * Platform identification value, which occupies first 8 bits in UUID.
     */
    enum PlatformID : unsigned char {
        Undefined       = 0b00000000,

        // Mobile
        iOS             = 0b00001001,
        Android         = 0b00001010,

        // PC
        Windows         = 0b00010000,
        Linux           = 0b00010001,
        OsX             = 0b00010011,
    };

    const uint8_t *value() const{
        return &_value[0];
    }

    bool isEmpty() {
        for (const auto &v : _value) {
            if (v != 0)
                return false;
        }
        return true;
    }

private:
    uint8_t _value[UUID_BYTES];
};

}   // Anyfi namespace

#endif //ANYFIMESH_CORE_UUID_H
