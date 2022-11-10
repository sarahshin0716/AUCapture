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

#ifndef ANYFIMESH_CORE_MESHEDGE_H
#define ANYFIMESH_CORE_MESHEDGE_H

#include <cstdint>
#include <cstddef>
#include <string>

#include "../../../Common/Uuid.h"
#include "../../../Common/Network/Buffer/NetworkBuffer.h"

/**
 * 두 노드 사이의 연결정보 클래스입니다.
 */
class MeshEdge {
public:
    MeshEdge() = default;
    MeshEdge(Anyfi::UUID firstUid, Anyfi::UUID secondUid);
    MeshEdge(Anyfi::UUID first_uid, Anyfi::UUID secondUid, uint64_t updateTime);
    MeshEdge(Anyfi::UUID first_uid, Anyfi::UUID secondUid, uint64_t updateTime, int32_t cost);

    /**
     * Setters
     */
    void cost(int32_t cost) { _cost = cost; }
    void updateTime(uint64_t updateTime) { _updateTime = updateTime; }
    void firstUid(Anyfi::UUID firstUid) { _firstUid = firstUid; }
    void secondUid(Anyfi::UUID secondUid) { _secondUid = secondUid; }

    /**
     * Getters
     */
    int32_t cost() const { return _cost; }
    uint64_t updateTime() const { return _updateTime; }
    Anyfi::UUID firstUid() const { return _firstUid; }
    Anyfi::UUID secondUid() const { return _secondUid; }

    /**
     * MeshEdge를 NetworkBuffer 형태로 패킹
     * 패킷형태
     * |---------------------------------------------|
     * | 4 bytes | 4 bytes  | 4 bytes   | 8 bytes    |
     * |---------------------------------------------|
     * | cost    | FirstUid | SecondUid | UpdateTime |
     * @return NetworkBufferPointer 패킹된 NetworkBuffer
     */
    NetworkBufferPointer toPacket();
    /**
     * MeshEdge를 NetworkBuffer 형태로 패킹. Param으로 온 buffer에 추가
     * @param buffer NetworkBuffer
     */
    void toPacket(NetworkBufferPointer buffer);
    /**
     * NetworkBuffer 형태로 패킹된 MeshEdge를 다시 unpacking
     * @param buffer 패킹된 MeshEdge가 포함된 NetworkBuffer
     * @return std::shared_ptr<MeshEdge>
     */
    static std::shared_ptr<MeshEdge> toSerialize(NetworkBufferPointer buffer);

    std::string toString() const;

    /**
     * Operator ==
     * UID 비교
     * @param other
     * @return FirstUID, SecondUID가 반대여도 UID가 서로 같을 경우 true
     */
    bool operator==(const MeshEdge & other) const;
    struct Equaler {
        bool operator() (const std::shared_ptr<MeshEdge>& lhs,
                         const std::shared_ptr<MeshEdge>& rhs) const;
    };

    struct Hasher {
        std::size_t operator()(const std::shared_ptr<MeshEdge>& k) const;
    };

private:
    int32_t _cost;
    Anyfi::UUID _firstUid;
    Anyfi::UUID _secondUid;
    uint64_t _updateTime;
};

#endif //ANYFIMESH_CORE_MESHEDGE_H
