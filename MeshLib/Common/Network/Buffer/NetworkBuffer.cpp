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

#include <iomanip>

#include "NetworkBuffer.h"


/**
 * NetworkBuffer Constructor.
 * Reserves specified size in internal vector.
 *
 * @param capacity Size (in bytes) of space to preallocate internally. Default is set in BB_DEFAULT_SIZE
 */
NetworkBuffer::NetworkBuffer(uint32_t capacity) : _capacity(capacity) {
    reset();

    _size = 0;

    _buf = new uint8_t[capacity];
}

/**
 * NetworkBuffer Constructor.
 * Consume an entire uint8_t array of length len in the NetworkBuffer.
 *
 * @param arr uint8_t array of data (should be a length len)
 * @param capacity Size of space to preallocate
 */
NetworkBuffer::NetworkBuffer(uint8_t *arr, uint32_t capacity) : _capacity(capacity) {
    reset();

    _size = 0;

    _buf = new uint8_t[capacity];

    // Put bytes only when the provided array is not null
    if (arr != nullptr) {
        putBytes(arr, capacity);
    }
}

/**
 * NetworkBufferPool destructor.
 */
NetworkBuffer::~NetworkBuffer() {
    delete []_buf;
};


/**
 * Set NetworkBuffer networkMode to the Backward.
 */
void NetworkBuffer::setBackwardMode() {
    _mode = NetworkBufferMode::Backward;
}

/**
 * Returns true if networkMode is Backward, false otherwise.
 */
bool NetworkBuffer::isBackwardMode() const {
    return _mode == NetworkBufferMode::Backward;
}

/**
 * Reset.
 * Reset positions and networkMode.
 */
void NetworkBuffer::reset() {
    _mode = NetworkBufferMode::Forward;
    _readPos = 0;
    _writePos = 0;
    _size = 0;
}

/**
 * Clone.
 * Allocate an exact copy of the NetworkBuffer on the heap and return a pointer.
 *
 * @return A pointer to the newly cloned NetworkBuffer. nullptr if no more memory available.
 */
//std::shared_ptr<NetworkBuffer> NetworkBuffer::clone() const {
//    std::shared_ptr<NetworkBuffer> newBuf = std::make_shared<NetworkBuffer>(_buf, _capacity);
//
//    // Set positions
//    newBuf->_mode = _mode;
//    newBuf->_readPos = _readPos;
//    newBuf->_writePos = _writePos;
//    newBuf->_size = _size;
//
//    return newBuf;
//}

void NetworkBuffer::copyFrom(const NetworkBuffer* buf)
{
    reset();

    _size = 0;

    putBytes(buf->_buf, buf->_capacity);

    // Set positions
    _mode = buf->_mode;
    _readPos = buf->_readPos;
    _writePos = buf->_writePos;
    _size = buf->_size;
}

/**
 * Equals.
 * Compare this NetworkBuffer to another by looking at each byte in the internal buffer and making sure they are the same.
 *
 * @param other A pointer to a NetworkBuffer to compare this one
 * @return true if the internal buffers match, false if otherwise
 */
bool NetworkBuffer::equals(NetworkBuffer *other) const {
    // If sizes aren't equal, they can't be equal.
    if (size() != other->size())
        return false;

    // Compare byte by byte
    for (uint32_t i = 0; i < size(); ++i) {
        if (get<uint8_t>(i) != other->get<uint8_t>(i))
            return false;
    }

    return true;
}

/**
 * Operator ==
 * Same as Equals
 *
 * @param other NetworkBuffer reference to compare
 * @return true if the interanl buffers match, false if otherwise
 */
bool NetworkBuffer::operator==(const NetworkBuffer &other) const {
    if (size() != other.size())
        return false;

    // Compare byte by byte
    for (uint32_t i = 0; i < size(); ++i) {
        if (get<uint8_t>(i) != other.get<uint8_t>())
            return false;
    }

    return true;
}


/**
 * Size.
 * Returns the size of internal buffer vector.
 *
 * @return size of the internal buffer.
 */
size_t NetworkBuffer::size() const {
    return _size;
}

/**
 * Resize.
 * Read and write position will also be reset to 0.
 *
 * @param newSize The amount of memory to allocate.
 */
void NetworkBuffer::resize(uint32_t newSize) {
    _size = newSize;
    _readPos = 0;
    _writePos = 0;
}

/**
 * Returns the capacity of internal buffer vecotr.
 */
size_t NetworkBuffer::capacity() const {
    return _capacity;
}

/**
 * Find a position where the key located in the internal buffer.
 *
 * @return position of the key in the internal buffer. -1 if not found in buffer.
 */
template<typename T>
int32_t NetworkBuffer::find(T key, uint32_t start) const {
    int32_t ret = -1;

    for (uint32_t i = start; i < _size; ++i) {
        T read_data = read<T>(i);

        // Key was found in buffer
        if (read_data == key) {
            return static_cast<int32_t>(i);
        }
    }

    return ret;
}

/**
 * Replace occurance of a particular uint8_t key, with the uint8_t replace.
 *
 * @param key uint8_t to find for replacement
 * @param replace uint8_t to replace the found key with
 * @param start Index to start from. By default, start is 0
 * @param firstOccuranceOnly If true, only replace the first occurance of the key. If false, repalce all occurances. False by default.
 */
void NetworkBuffer::replace(uint8_t key, uint8_t replace, uint32_t start, bool firstOccuranceOnly) {
    for (uint32_t i = start; i < _size; ++i) {
        uint8_t read_data = read<uint8_t>(i);

        // Wasn't actually found, boudns of buffer were exceeded
        if (key != NULL && read_data == NULL)
            break;

        // Key was found in array, perform replacement
        if (read_data == key) {
            _buf[i] = replace;

            if (firstOccuranceOnly)
                return;
        }
    }
}

/**
 * Read bytes and set to dst.
 * If you set index to a value equal to or greater than zero, it does not change the read position.
 *
 * @param dst The array into which bytes are to be written.
 * @param len Length of the array dst.
 * @param index Index to read bytes.
 */
void NetworkBuffer::getBytes(uint8_t *dst, uint32_t len, int index) {
    if (index < 0) {
        uint8_t *pos;
        if (isBackwardMode())
            pos = _buf + _capacity - _readPos - len;
        else pos = _buf + _readPos;

        memcpy(dst, pos, len);
        _readPos += len;
        return;
    }

    uint8_t *pos;
    if (isBackwardMode())
        pos = _buf + _capacity - index - len;
    else pos = _buf + index;

    memcpy(dst, pos, len);
}

/**
 * Gets a pointer of the internal buffer.
 */
uint8_t *NetworkBuffer::buffer() const {
    auto pointer = _buf;
    if (isBackwardMode())
        pointer += capacity() - _size;
    return pointer;
}

/**
 * Put bytes from the given NetworkBuffer.
 * If you set index to a value equal to or greater than zero, it does not change the read position.
 */
void NetworkBuffer::putBytes(NetworkBuffer *src, int index) {
    putBytes(src->buffer(), static_cast<uint32_t>(src->size()), index);
}

/**
 * Put bytes from the given const char array.
 * If you set index to a value equal to or greater than zero, it does not change the read position.
 */
void NetworkBuffer::putBytes(const char *b, int index) {
    putBytes((uint8_t *) b, strlen(b), index);
}

/**
 * Put bytes from the uint8_t array pointer.
 * If you set index to a value equal to or greater than zero, it does not change the read position.
 */
void NetworkBuffer::putBytes(uint8_t *b, size_t len, int index) {
    if (index >= 0) {   // Absolute putBytes
        if (index < _readPos)
            throw NETWORKBUFFER_ERROR_INVALID_PUT_IDX;
        if (index + len > capacity())
            throw NETWORKBUFFER_ERROR_CAPACITY_EXCEEDED;
        if (index + len > size())
            _size = index + len;

        if (isBackwardMode())
            index = (uint32_t) (_capacity - index - len);

        memcpy(_buf + index, b, len);
    } else {            // Relative putBytes
        if (_writePos + len > capacity())
            throw NETWORKBUFFER_ERROR_CAPACITY_EXCEEDED;
        if (_writePos + len > size()) {
            _size = _writePos + len;
        }

        uint8_t *pos;
        if (isBackwardMode())
            pos = _buf + _capacity - _writePos - len;
        else pos = _buf + _writePos;


        memcpy(pos, b, len);

        _writePos += len;
    }
}

std::string NetworkBuffer::printAsHex() {
    uint8_t *point = buffer();
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << std::hex;
    if (isBackwardMode()) {
        for (int i = 0; i < getWritePos(); ++i) {
            ss << (int) point[i];
            ss << " ";
        }
        ss << std::endl;
        for (int i = getWritePos() - 1; i > 0; i--) {
            ss << (int) point[i];
            ss << " ";
        }
    } else {
        for (int i = 0; i < getWritePos(); ++i) {
            ss << (int) point[i];
            ss << " ";
        }
    }
    return ss.str();
}
