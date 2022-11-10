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

#ifndef ANYFIMESH_CORE_MESHNODE_H
#define ANYFIMESH_CORE_MESHNODE_H

#include <string>

#include "../../Common/Uuid.h"

/**
 * MeshNetwork에서 Node의 정보를 담고있는 클래스입니다.
 */
class MeshNode {
public:
    explicit MeshNode() = default;
    MeshNode(Anyfi::UUID uid, const std::string &deviceName);
    MeshNode(Anyfi::UUID uid, const std::string &deviceName, uint8_t type);
    MeshNode(Anyfi::UUID uid, const std::string &deviceName, uint8_t type, uint64_t updateTime);

    /**
     * Setters
     */
    void uid(Anyfi::UUID uid) { _uid = uid; }
    void deviceName(const std::string &deviceName) { _deviceName = deviceName; }
    void type(uint8_t type) { _type = type; }
    void updateTime(uint64_t updateTime) { _updateTime = updateTime; }

    /**
     * Getters
     */
    Anyfi::UUID uid() { return _uid; }
    std::string deviceName() { return _deviceName; }
    uint8_t type() { return _type; }
    uint64_t updateTime() { return _updateTime; }

    /**
     * MeshNode를 NetworkBuffer 형태로 패킹
     * 패킷 형태
     * |-----------------------------------------------------------|
     * | 4 byte | 4 byte        | N Byte     | 1 Byte | 8 Byte     |
     * |-----------------------------------------------------------|
     * | UID    | DeviceNameSize| DeviceName | Type   | UpdateTime |
     * |-----------------------------------------------------------|
     * @return NetworkBufferPointer 패킹된 NetworkBuffer
     */
    NetworkBufferPointer toPacket();
    /**
     * MeshNode를 NetworkBuffer 형태로 패킹. Parameter로 온 buffer에 추가
     * @param buffer NetworkBuffer
     */
    void toPacket(NetworkBufferPointer buffer);

    /**
     * NetworkBuffer 형태로 패킹된 MeshNode를 다시 unpacking
     * @param buffer 패킹된 MeshNode가 포함된 NetworkBuffer
     * @return
     */
    static std::shared_ptr<MeshNode> toSerialize(NetworkBufferPointer buffer);

    std::string toString() const;
private:
    Anyfi::UUID _uid;
    std::string _deviceName;
    uint8_t _type;
    uint64_t _updateTime;
};


#endif //ANYFIMESH_CORE_MESHNODE_H
