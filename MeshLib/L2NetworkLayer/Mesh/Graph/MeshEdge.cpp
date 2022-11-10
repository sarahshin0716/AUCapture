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

#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "Common/Uuid.h"
#include "MeshEdge.h"

MeshEdge::MeshEdge(Anyfi::UUID firstUid, Anyfi::UUID secondUid) {
    _firstUid = firstUid;
    _secondUid = secondUid;
    _updateTime = 0;
    _cost = 0;
}

MeshEdge::MeshEdge(Anyfi::UUID firstUid, Anyfi::UUID secondUid, uint64_t updateTime) {
    _firstUid = firstUid;
    _secondUid = secondUid;
    _updateTime = updateTime;
    _cost = 0;
}

MeshEdge::MeshEdge(Anyfi::UUID firstUid, Anyfi::UUID secondUid, uint64_t updateTime, int32_t cost) {
    _firstUid = firstUid;
    _secondUid = secondUid;
    _updateTime = updateTime;
    _cost = cost;
}

NetworkBufferPointer MeshEdge::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();
    toPacket(buffer);
    return buffer;
}

void MeshEdge::toPacket(NetworkBufferPointer buffer) {
    if (buffer->isBackwardMode()) {
        buffer->put<uint64_t>(_updateTime);
        _secondUid.toPacket(buffer);
        _firstUid.toPacket(buffer);
        buffer->put<int32_t>(_cost);
    } else {
        buffer->put<int32_t>(_cost);
        _firstUid.toPacket(buffer);
        _secondUid.toPacket(buffer);
        buffer->put<uint64_t>(_updateTime);
    }
}

std::shared_ptr<MeshEdge> MeshEdge::toSerialize(NetworkBufferPointer buffer) {
    auto newMeshEdge = std::make_shared<MeshEdge>();

    // Cost
    newMeshEdge->cost(buffer->get<int32_t>());

    // First UID
    auto firstUid = Anyfi::UUID(buffer);
    newMeshEdge->firstUid(firstUid);

    // Second UID
    auto secondUid = Anyfi::UUID(buffer);
    newMeshEdge->secondUid(secondUid);

    // Update Time
    newMeshEdge->updateTime(buffer->get<uint64_t>());
    return newMeshEdge;
}

std::string MeshEdge::toString() const {
    std::string desc = "{ first_uid: ";
    desc.append(_firstUid.toString());
    desc.append(", second_uid: ");
    desc.append(_secondUid.toString());
    desc += ", cost: " + to_string(_cost)
            + ", update_time: " + to_string(_updateTime)
            + "}";
    return desc;
}


bool MeshEdge::operator==(const MeshEdge &other) const {
    return ((this->firstUid() == other.firstUid()) && (this->secondUid() == other.secondUid()))
           || ((this->firstUid() == other.secondUid()) && (this->secondUid() == other.firstUid()));
}

bool MeshEdge::Equaler::operator()(const std::shared_ptr<MeshEdge> &lhs,
                                   const std::shared_ptr<MeshEdge> &rhs) const {
    return ((lhs->firstUid() == rhs->firstUid()) && (lhs->secondUid() == rhs->secondUid()))
           || ((lhs->firstUid() == rhs->secondUid()) && (lhs->secondUid() == rhs->firstUid()));
}

std::size_t MeshEdge::Hasher::operator()(const std::shared_ptr<MeshEdge> &k) const {
    const int prime = 31;
    size_t result = 1;
    size_t firstUidSum = 0;
    size_t secondUidSum = 0;
    for (int i = 0; i < UUID_BYTES; i++) {
        firstUidSum += k->firstUid().value()[i];
        secondUidSum += k->secondUid().value()[i];
    }
    result = prime * result + firstUidSum;
    result = prime * result + secondUidSum;
    return result;
}