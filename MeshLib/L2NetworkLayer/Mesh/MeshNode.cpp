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

#include "Common/Uuid.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "MeshNode.h"


MeshNode::MeshNode(Anyfi::UUID uid, const std::string &deviceName) : MeshNode(uid, deviceName, 0, 0) { }
MeshNode::MeshNode(Anyfi::UUID uid, const std::string &deviceName, uint8_t type) : MeshNode(uid, deviceName, type, 0) { }
MeshNode::MeshNode(Anyfi::UUID uid, const std::string &deviceName, uint8_t type, uint64_t updateTime) {
    _uid = uid;
    _deviceName = deviceName;
    _type = type;
    _updateTime = updateTime;
}

std::string MeshNode::toString() const {
    std::string desc = "{ uid:" + _uid.toString();
    desc.append(", device_name:'" + to_string(_deviceName) + "'");
    desc.append(", type:"+to_string((short)_type));
    desc.append(", update_time:" + to_string(_updateTime));
    desc.append(" }");
    return desc;
}

NetworkBufferPointer MeshNode::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();
    toPacket(buffer);
    return buffer;
}

void MeshNode::toPacket(NetworkBufferPointer buffer) {
    if (buffer->isBackwardMode()) {
        buffer->put<uint64_t>(_updateTime);
        buffer->put<uint8_t>(_type);
        buffer->putBytes((uint8_t *) _deviceName.c_str(), (uint32_t) _deviceName.size());
        buffer->put<uint32_t>((uint32_t) _deviceName.size());
        _uid.toPacket(buffer);
    }else {
        _uid.toPacket(buffer);
        buffer->put<uint32_t>((uint32_t) _deviceName.size());
        buffer->putBytes((uint8_t *) _deviceName.c_str(), (uint32_t) _deviceName.size());
        buffer->put<uint8_t>(_type);
        buffer->put<uint64_t>(_updateTime);
    }
}

std::shared_ptr<MeshNode> MeshNode::toSerialize(NetworkBufferPointer buffer) {
    auto meshNode = std::make_shared<MeshNode>();

    auto uid = Anyfi::UUID(buffer);
    auto deviceNameSize = buffer->get<uint32_t>();
    std::vector<uint8_t> deviceNameVec(deviceNameSize);
    buffer->getBytes(&deviceNameVec[0], deviceNameSize);
    std::string deviceName(reinterpret_cast<char const *>(&deviceNameVec[0]), deviceNameSize);
    auto type = buffer->get<uint8_t>();
    auto updateTime = buffer->get<uint64_t>();
    meshNode->uid(uid);
    meshNode->deviceName(deviceName);
    meshNode->type(type);
    meshNode->updateTime(updateTime);
    return meshNode;
}