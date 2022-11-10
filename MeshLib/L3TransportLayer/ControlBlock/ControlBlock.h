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

#ifndef ANYFIMESH_CORE_CONTROLBLOCK_H
#define ANYFIMESH_CORE_CONTROLBLOCK_H

#include "../../Common/Network/Address.h"
#include "../../Common/DataStructure/LRUCache.h"
#include "../../Common/stdafx.h"

#include <utility>

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif


// Forward declarations : self
class ControlBlock;
class ControlBlockKey;


/**
 * The key for {@link ControlBlock}.
 */
class ControlBlockKey {
public:
    ControlBlockKey() {}
    ControlBlockKey(Anyfi::Address src, Anyfi::Address dst)
            : _srcAddress(std::move(src)), _dstAddress(std::move(dst)) {}

    bool operator==(const ControlBlockKey &other) const;
    bool operator!=(const ControlBlockKey &other) const;

    Anyfi::Address srcAddress() const { return _srcAddress; }
    Anyfi::Address dstAddress() const { return _dstAddress; }

    struct Hasher {
        std::size_t operator()(const ControlBlockKey &k) const;
    };

private:
    Anyfi::Address _srcAddress;
    Anyfi::Address _dstAddress;
};


/**
 * The ControlBlock it the block of data which describes a network connection.
 * This class used as a base abstract class for {@link TCB} and {@link RUCB}.
 *
 * The id assignment is done in L3TransportLayer when received a control block from PacketProcessor.
 */
class ControlBlock {
public:
    ControlBlock() {}
    ControlBlock(ControlBlockKey key) : _key(std::move(key)) {}
    virtual ~ControlBlock() = default;

    const unsigned short id() const { return _id; }
    ControlBlockKey &key() { return _key; }

protected:
    unsigned short _id;
    ControlBlockKey _key;

    friend class L3TransportLayer;
    void assignID(unsigned short id) {
        _id = id;
    }

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(MeshUDPProcessor, UDPReadWrite);
    FRIEND_TEST(MeshRUDPProcessor, Connect);
    FRIEND_TEST(MeshRUDPProcessor, ReadWrite);
    FRIEND_TEST(MeshRUDPProcessor, ReadWriteRandomDrop);
    FRIEND_TEST(MeshRUDPProcessor, ReadPerformance);
    FRIEND_TEST(MeshRUDPProcessor, WritePerformance);
    FRIEND_TEST(MeshRUDPProcessor, Close);
    FRIEND_TEST(MeshRUDPProcessor, Shutdown);
    FRIEND_TEST(MeshEnetRUDPProcessor, Connect);
    FRIEND_TEST(MeshEnetRUDPProcessor, ReadWrite);
    FRIEND_TEST(MeshEnetRUDPProcessor, ReadWriteRandomDrop);
    FRIEND_TEST(MeshEnetRUDPProcessor, ReadPerformance);
    FRIEND_TEST(MeshEnetRUDPProcessor, WritePerformance);
    FRIEND_TEST(MeshEnetRUDPProcessor, Close);
    FRIEND_TEST(MeshEnetRUDPProcessor, Shutdown);
    friend class PacketProcessorTester;
#endif
};


#endif //ANYFIMESH_CORE_CONTROLBLOCK_H
