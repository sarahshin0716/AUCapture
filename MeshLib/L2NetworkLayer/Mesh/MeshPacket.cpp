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

#include "MeshPacket.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"

MeshPacket::Type MeshPacket::getType(NetworkBufferPointer buffer) {
    return static_cast<MeshPacket::Type>(buffer->get<uint8_t>(buffer->getReadPos()));
}
uint32_t MeshPacket::getPacketLength(NetworkBufferPointer buffer) {
    return buffer->get<uint32_t>(buffer->getReadPos()+ sizeof(uint8_t));
}


/**
 * -----------------------------------
 * MeshHandshakePacket
 */

NetworkBufferPointer MeshHandshakePacket::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();
    toPacket(buffer);
    return buffer;
}

void MeshHandshakePacket::toPacket(NetworkBufferPointer buffer) {
    buffer->put<uint64_t>(_updateTime);
    buffer->putBytes((uint8_t *)_deviceName.c_str(), (uint32_t) _deviceName.size());
    buffer->put<uint32_t>((uint32_t)_deviceName.size());
    _graphInfo->toPacket(buffer);
    _destUid.toPacket(buffer);
    _sourceUid.toPacket(buffer);
    buffer->put<uint32_t>(buffer->getWritePos() - buffer->getReadPos() + sizeof(uint8_t) + sizeof(uint32_t));
    uint8_t type = _type;
    buffer->put<uint8_t>(type);
}

std::shared_ptr<MeshHandshakePacket> MeshHandshakePacket::toSerialize(const NetworkBufferPointer &buffer, uint32_t offset) {
    auto handsPacket = std::make_shared<MeshHandshakePacket>();
    buffer->setReadPos(offset);
    auto type = static_cast<MeshPacket::Type>(buffer->get<uint8_t>());
    auto packetLength = buffer->get<uint32_t>();
    Anyfi::UUID sourceUid = Anyfi::UUID(buffer);
    Anyfi::UUID destUid = Anyfi::UUID(buffer);
    std::shared_ptr<GraphInfo> graphInfo = GraphInfo::toSerialize(buffer);

    /**
     * Buffer에서 DeviceName String 가져오기
     * std::vector 형태의 ByteArray로 가져온 다음, <char const *> 로 캐스팅 해서 std::string 변환
     */
    auto deviceNameSize = buffer->get<uint32_t>();
    std::vector<uint8_t> deviceNameVec(deviceNameSize);
    buffer->getBytes(&deviceNameVec[0], deviceNameSize);
    std::string deviceName(reinterpret_cast<char const *>(&deviceNameVec[0]), deviceNameSize);

    auto updateTime = buffer->get<uint64_t>();

    handsPacket->type(type);
    handsPacket->sourceUid(sourceUid);
    handsPacket->destUid(destUid);
    handsPacket->graphInfo(graphInfo);
    handsPacket->deviceName(deviceName);
    handsPacket->updateTime(updateTime);
    return handsPacket;
}

/**
 * -----------------------------------
 * MeshDatagramPacket
 */

NetworkBufferPointer MeshDatagramPacket::toPacket() {
    auto buffer = std::make_shared<NetworkBuffer>();
    buffer->setBackwardMode();
    buffer->putBytes(_payload.get());
    toPacket(buffer);
    return buffer;
}
void MeshDatagramPacket::toPacket(NetworkBufferPointer buffer) {
    if(!buffer->isBackwardMode())
        throw std::invalid_argument("NetworkBuffer must be backward networkMode");
    buffer->put<uint32_t>(buffer->getWritePos() - buffer->getReadPos());
    _destUid.toPacket(buffer);
    _sourceUid.toPacket(buffer);
    buffer->put<uint32_t>(buffer->getWritePos() - buffer->getReadPos() + sizeof(uint8_t) + sizeof(uint32_t));
    uint8_t type = _type;
    buffer->put<uint8_t>(type);
}

std::shared_ptr<MeshDatagramPacket> MeshDatagramPacket::toSerialize(const NetworkBufferPointer &buffer) {
    auto type = static_cast<MeshPacket::Type>(buffer->get<uint8_t>());
    auto packetLength = buffer->get<uint32_t>();
    Anyfi::UUID sourceUid = Anyfi::UUID(buffer);
    Anyfi::UUID destUid = Anyfi::UUID(buffer);
    buffer->setReadPos(buffer->getReadPos() + sizeof(uint32_t)); // payload size
    auto datagramPacket = std::make_shared<MeshDatagramPacket>();
    datagramPacket->type(type);
    datagramPacket->sourceUid(sourceUid);
    datagramPacket->destUid(destUid);
    datagramPacket->payload(buffer);
    return datagramPacket;
}

uint32_t MeshDatagramPacket::getPayloadSize(NetworkBufferPointer buffer) {
    return buffer->get<uint32_t>(buffer->getReadPos() + 37);
}

/**
 * -----------------------------------
 * MeshUpdatePacket
 */
NetworkBufferPointer MeshUpdatePacket::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();

    if (_meshEdge == nullptr) {
        auto uid = Anyfi::UUID();
        _meshEdge = std::make_shared<MeshEdge>(uid, uid);
    }
    _meshEdge->toPacket(buffer);

    if (_graphInfo == nullptr) {
        _graphInfo = std::make_shared<GraphInfo>();
    }
    _graphInfo->toPacket(buffer);

    buffer->put<uint64_t>(_updateTime);

    _destUid.toPacket(buffer);
    _sourceUid.toPacket(buffer);
    buffer->put<uint32_t>(buffer->getWritePos() - buffer->getReadPos() + sizeof(uint8_t) + sizeof(uint32_t));
    uint8_t type = _type;
    buffer->put<uint8_t>(type);

    return buffer;
}

std::shared_ptr<MeshUpdatePacket> MeshUpdatePacket::toSerialize(const NetworkBufferPointer &buffer) {
    auto type = static_cast<MeshPacket::Type>(buffer->get<uint8_t>());
    auto packetLength = buffer->get<uint32_t>();
    Anyfi::UUID sourceUid = Anyfi::UUID(buffer);
    Anyfi::UUID destUid = Anyfi::UUID(buffer);
    auto updateTime = buffer->get<uint64_t>();
    std::shared_ptr<GraphInfo> graphInfo = GraphInfo::toSerialize(buffer);
    std::shared_ptr<MeshEdge> meshEdge = MeshEdge::toSerialize(buffer);

    return std::make_shared<MeshUpdatePacket>(type, sourceUid, destUid, graphInfo, meshEdge, updateTime);
}

/**
 * -----------------------------------
 * MeshHeartBeatPacket
 */

NetworkBufferPointer MeshHeartBeatPacket::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();
    _destUid.toPacket(buffer);
    _sourceUid.toPacket(buffer);
    buffer->put<uint32_t>(buffer->getWritePos() - buffer->getReadPos() + sizeof(uint8_t) + sizeof(uint32_t));
    uint8_t type = _type;
    buffer->put<uint8_t>(type);
    return buffer;
}

std::shared_ptr<MeshHeartBeatPacket> MeshHeartBeatPacket::toSerialize(const NetworkBufferPointer &buffer) {
    auto heartbeatPacket = std::make_shared<MeshHeartBeatPacket>();
    auto type = static_cast<MeshPacket::Type>(buffer->get<uint8_t>());
    auto packetLength = buffer->get<uint32_t>();
    Anyfi::UUID sourceUid = Anyfi::UUID(buffer);
    Anyfi::UUID destUid = Anyfi::UUID(buffer);

    heartbeatPacket->type(type);
    heartbeatPacket->sourceUid(sourceUid);
    heartbeatPacket->destUid(destUid);

    return heartbeatPacket;
}