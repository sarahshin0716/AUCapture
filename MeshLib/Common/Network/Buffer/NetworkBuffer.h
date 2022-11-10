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

#ifndef ANYFIMESH_CORE_NETWORKBUFFER_H
#define ANYFIMESH_CORE_NETWORKBUFFER_H

#define NB_DEFAULT_SIZE 16834

#pragma warning(disable: 4345)

#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <vector>
#include <memory>
#include <iostream>
#include <cstdio>

#include "../../../Common/CallbackableSharedPtr.h"
#include "../../../Log/Frontend/Log.h"
#include "Endian.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif


#define NETWORKBUFFER_ERROR_CAPACITY_EXCEEDED std::range_error("Capacity exceeded");
#define NETWORKBUFFER_ERROR_INVALID_PUT_IDX std::invalid_argument("Put index cannot be smaller than read position");


enum NetworkBufferMode : bool {
    /**
     * Uses a NetworkBuffer in the forward direction.
     * Get/Puts the data from the beginning of the NetworkBuffer.
     *
     * It's default networkMode of the NetworkBuffer.
     * A Common way to use NetworkBuffer.
     */
    Forward     = true,

    /**
     * Uses a NetworkBuffer in the backward direction.
     * Get/Puts the data from the end of the NetworkBuffer.
     *
     * It is used to create packet from the upper layer and send it to the lower layer.
     * In this networkMode, it's possible to add a buffer before the existing buffer.
     * This is useful when adding headers before a payload.
     *
     * In the Backward networkMode, it behaves differently from the Forward networkMode.
     * First of all, counts the read/write position from the back.
     * Secondly, the size of the internal buffer vector is always equal to capacity, and NetworkBuffer#size() returns _backwardSize.
     * If you resize NetworkBuffer, the internal buffer vector remains unchanged and only the _backwardSize changes.
     * Lastly, NetworkBuffer#buffer() returns a pointer of the write position.
     */
    Backward    = false,
};


/**
 * This NetworkBuffer class was written to provide Java's NetworkBuffer functionality.
 */
class NetworkBuffer {
public:
    /** NetworkBuffer Constructor & Destructor **/
    NetworkBuffer(uint32_t capacity = NB_DEFAULT_SIZE);
    NetworkBuffer(uint8_t* arr, uint32_t capacity);
    ~NetworkBuffer();

    /**
     * Read and returns a value which locate the current read position.
     * It does not increment the read position.
     */
    template<typename T>
    T peek() const {
        return read<T>(_readPos);
    }

    /**
     * Read and returns a value.
     * If you set index to a value equal to or greater than zero, it does not change the read position.
     */
    template<typename T>
    T get(int index = -1) const {
        if (index < 0) return read<T>();
        return read<T>(index);
    }

    /**
     * Write a value to the internal buffer.
     * If you set index equal to or greater than zero, it does not change write position.
     */
    template<typename T>
    void put(T &value, int index = -1) {
        if (index >= 0) {   // Absolute put
            insert<T>(value, index);
        } else {            // Relative put
            append<T>(value);
        }
    }

    /**
     * Write a value to the internal buffer.
     * If you set index equal to or greater than zero, it does not change write position.
     */
    template<typename T>
    void put(T &&value, int index = -1) {
        put<T>(value, index);
    }

    /** Base NetworkBuffer Methods **/
    void setBackwardMode();
    bool isBackwardMode() const;
    void reset();                                   // Reset position and networkMode.
    //std::shared_ptr<NetworkBuffer> clone() const;   // Return a new instance of a NetworkBuffer with the exact same contnets and the same state
    void copyFrom(const NetworkBuffer* buf);
    bool equals(NetworkBuffer* other) const;           // Compare if the contents are equivalent
    bool operator==(const NetworkBuffer& other) const; // Operator Overloading(==)
    void resize(uint32_t newSize);                  // Resize NetworkBuffer
    size_t size() const;                            // Size of internal vector
    size_t capacity() const;                        // Capacity of internal vector
    template<typename T> int32_t find(T key, uint32_t start = 0) const;                                 // Basic Searching (Linear)
    void replace(uint8_t key, uint8_t replace, uint32_t start = 0, bool firstOccuranceOnly = false);    // Replacement

    /** Read Methods **/
    void getBytes(uint8_t* dst, uint32_t len, int index = -1);
    uint8_t *buffer() const;

    /** Write Methods **/
    void putBytes(NetworkBuffer* src, int index = -1);
    void putBytes(const char *b, int index = -1);
    void putBytes(uint8_t* b, size_t len, int index = -1);

    /** Buffer Position Accessors & Mutators **/
    void setReadPos(uint32_t readPos) {
        if (readPos > _writePos)
            _writePos = readPos;
        _readPos = readPos;
    }
    uint32_t getReadPos() const { return _readPos; }
    void setWritePos(uint32_t writePos) {
        if (writePos < _readPos)
            _readPos = writePos;
        _writePos = writePos;
    }
    uint32_t getWritePos() const { return _writePos; }

    std::string printAsHex();

private:
    NetworkBufferMode _mode = NetworkBufferMode::Forward;

    uint32_t _writePos;
    mutable uint32_t _readPos;

    uint32_t _size = 0;
    const uint32_t _capacity = 0;

    uint8_t *_buf;

    template<typename T>
    T read() const {
        if (_readPos + sizeof(T) > _writePos) {
            return NULL;
        }

        T data = read<T>(_readPos);
        _readPos += sizeof(T);
        return data;
    }

    template<typename T>
    T read(int index) const {
        if (index + sizeof(T) <= _size) {
            if (isBackwardMode())
                index = (uint32_t) (_capacity - index - sizeof(T));
            return Endian::networkToHostOrder<T>(*((T *) &_buf[index]));
        }
        return NULL;
    }

    template<typename T>
    void append(T &data) {
        uint32_t data_size = sizeof(data);

        if (capacity() < (_writePos + data_size))
            throw NETWORKBUFFER_ERROR_CAPACITY_EXCEEDED;
        if (size() < (_writePos + data_size))
            _size = _writePos + data_size;

        uint32_t pos = 0;
        if (isBackwardMode())
            pos = (uint32_t) (_capacity - _writePos - sizeof(T));
        else pos = _writePos;

        T networkData = Endian::hostToNetworkOrder<T>(data);
        memcpy(&_buf[pos], reinterpret_cast<uint8_t*>(&networkData), data_size);

        _writePos += data_size;
    }

    template<typename T>
    void insert(T &data, int index) {
        if (index < _readPos)
            throw NETWORKBUFFER_ERROR_INVALID_PUT_IDX;
        if (capacity() < (index + sizeof(data)))
            throw NETWORKBUFFER_ERROR_CAPACITY_EXCEEDED;
        if (size() < (index + sizeof(data)))
            _size = index + sizeof(data);

        if (isBackwardMode())
            index = (uint32_t) (_capacity - index - sizeof(T));

        T networkData = Endian::hostToNetworkOrder<T>(data);
        memcpy(&_buf[index], reinterpret_cast<uint8_t*>(&networkData), sizeof(data));
    }


#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(NetworkBuffer, BackwardBufferPointer);
    FRIEND_TEST(NetworkBuffer, BackwardRelativePut);
    FRIEND_TEST(NetworkBuffer, BcakwardAbsolutePut);
    FRIEND_TEST(NetworkBuffer, BackwardGetPutBytes);
#endif
};

//typedef CallbackableSharedPtr<NetworkBuffer> NetworkBufferPointer;
typedef std::shared_ptr<NetworkBuffer> NetworkBufferPointer;

#endif //ANYFIMESH_CORE_NETWORKBUFFER_H
