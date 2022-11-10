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

#ifndef ANYFIMESH_CORE_MESHPACKET_H
#define ANYFIMESH_CORE_MESHPACKET_H

#include <string>
#include <stdexcept>

#include "../../Common/Network/Buffer/NetworkBuffer.h"
#include "../../Common/Network/Address.h"
#include "../../L2NetworkLayer/Mesh/Graph/GraphInfo.h"

/**
 * L2NetworkLayer에서의 통신을 위한 프로토콜입니다.
 */
/**
 * MeshPacket 공통 형태
 * |--------------------------------|
 * | 1 byte | 4 bytes      | ...... |
 * |--------------------------------|
 * | Type   | PacketLength | ...... |
 * |--------------------------------|
 */
class MeshPacket {
public:
    enum Type : uint8_t {
        Datagram = 1,
        HandshakeReq = 2,
        HandshakeReqRes = 3,
        HandshakeRes = 4,
        GraphUpdateReq = 5,
        GraphUpdateRes = 6,
        HeartbeatReq = 7,
        HeartbeatRes = 8,
    };

    static std::string toString(Type type) {
        switch (type) {
            case Datagram:
                return "Datagram";
            case HandshakeReq:
                return "HandshakeReq";
            case HandshakeReqRes:
                return "HandshakeReqRes";
            case HandshakeRes:
                return "HandshakeRes";
            case GraphUpdateReq:
                return "GraphUpdateReq";
            case GraphUpdateRes:
                return "GraphUpdateRes";
            case HeartbeatReq:
                return "HeartbeatReq";
            case HeartbeatRes:
                return "HeartbeatRes";
            default:
                return "Unknown";
        }
        return "Unknown";
    }

    MeshPacket() : _sourceUid(Anyfi::UUID()), _destUid(Anyfi::UUID()) {};
    ~MeshPacket() = default;

    /**
     * Setters
     */
    void type(Type type) { _type = type; }
    void sourceUid(Anyfi::UUID uid) { _sourceUid = uid; }
    void destUid(Anyfi::UUID uid) { _destUid = uid; }

    /**
     * Getters
     */
    Type type() { return _type; }
    Anyfi::UUID sourceUid() { return _sourceUid; }
    Anyfi::UUID destUid() { return _destUid; }

    // 모든 MeshPacket 상속 클래스가 NetworkBuffer 형식으로 packing, unpacking을 하기위한 virtual method입니다.
    /**
     * NetworkBuffer 형태로 Packing합니다.
     * @return NetworkBufferPointer
     */
    virtual NetworkBufferPointer toPacket()=0;
    /**
     * NetworkBuffer 형태로 변환된 MeshPacket을 다시 클래스 형태로 되돌립니다.
     * @return std::shared_ptr<MeshPacket>
     */
//   virtual std::shared_ptr<{MeshPacket}> toSerialize() { return nullptr; };

    /**
     * NetworkBuffer가 어떤 타입의 MeshPacket인지 반환합니다.
     * @param buffer NetworkBuffer로 변환된 MeshPacket
     * @return MeshPacket::Type
     */
    static MeshPacket::Type getType(NetworkBufferPointer buffer);
    static uint32_t getPacketLength(NetworkBufferPointer buffer);
protected:
    Type _type;
    Anyfi::UUID _sourceUid;
    Anyfi::UUID _destUid;
};

/**
 * MeshHandshake를 위한 프로토콜입니다.
 * Handshake 도중 주고받는 GraphInfo, UpdateTime 등을 포함하고 있습니다.
 */
class MeshHandshakePacket : public MeshPacket {
public:
    explicit MeshHandshakePacket() = default;
    MeshHandshakePacket(Type type, Anyfi::UUID sourceUid, Anyfi::UUID destUid,
                        const std::shared_ptr<GraphInfo> &graphInfo,
                        uint64_t updateTime, const std::string &deviceName) {
        _type = type;
        _sourceUid = sourceUid;
        _destUid = destUid;
        _graphInfo = graphInfo;
        _updateTime = updateTime;
        _deviceName = deviceName;
    }

    /**
     * Setters
     */
    void graphInfo(const std::shared_ptr<GraphInfo> &graphInfo) { _graphInfo = graphInfo; }
    void updateTime(uint64_t updateTime) { _updateTime = updateTime; }
    void deviceName(const std::string &deviceName) { _deviceName = deviceName; }

    /**
     * Getters
     */
    std::shared_ptr<GraphInfo> graphInfo() const { return _graphInfo; }
    uint64_t updateTime() const { return _updateTime; }
    std::string deviceName() const { return _deviceName; }

    /**
     * Override : MeshPacket
     */
    /**
     * 패킷 형태
     * |------------------------------------------------------------------------------------------------------|
     * | 1 byte | 4 bytes      | 16 bytes   | 16 bytes | N Bytes   | 4 Bytes        | N Bytes    | 8 bytes    |
     * |------------------------------------------------------------------------------------------------------|
     * | Type   | PacketLength | SourceUid  | DestUid  | GraphInfo | DeviceNameSize | DeviceName | UpdateTime |
     * |------------------------------------------------------------------------------------------------------|
     */
    NetworkBufferPointer toPacket() override;
    void toPacket(NetworkBufferPointer buffer);

    static std::shared_ptr<MeshHandshakePacket> toSerialize(const NetworkBufferPointer &buffer, uint32_t offset);

    std::string toString() {
        std::string result = "{ type:" + MeshPacket::toString(type())
                             + ", source:" + sourceUid().toString()
                             + ", dest:" + destUid().toString()
                             + ", graphInfo:" + ((graphInfo()) ? graphInfo()->toString() : std::string("null"))
                             + ", deviceName:'" + deviceName() + "'"
                             + ", updateTime:" + to_string(updateTime()) + " }";
        return result;
    }

private:
    // 상대방에게 보낼 GraphInfo
    std::shared_ptr<GraphInfo> _graphInfo;
    // 새로 생성할 MeshEdge의 updateTime (ReqRes, Res에서만 사용)
    uint64_t _updateTime = 0;
    // 송신자 자신의 DeviceName
    std::string _deviceName = "";
    // 자신의 MeshNodeType
    uint8_t _nodeType = 0;
};

/**
 * MeshNetwork에서 일반적인 패킷 송수신을 위한 프로토콜입니다.
 */
class MeshDatagramPacket : public MeshPacket {
public:
    MeshDatagramPacket(Anyfi::UUID sourceUid, Anyfi::UUID destUid, NetworkBufferPointer payload) {
        _sourceUid = sourceUid;
        _destUid = destUid;
        _payload = payload;
        _type = Type::Datagram;
    }
    MeshDatagramPacket(Anyfi::UUID sourceUid, Anyfi::UUID destUid) : MeshDatagramPacket(sourceUid, destUid, nullptr) {}
    explicit MeshDatagramPacket() : MeshDatagramPacket(Anyfi::UUID(), Anyfi::UUID(), nullptr) { };
    /**
     * Getter & setter
     */
    void payload(const NetworkBufferPointer &payload) { _payload = payload; }
    NetworkBufferPointer payload() const { return _payload; };

    /**
     * Override : MeshPacket
     */
    /**
     * 패킷 형태
     * |--------------------------------------------------------------------------------|
     * | 1 byte | 4 bytes      | 16 bytes   | 16 bytes | 4 bytes      | N Bytes         |
     * |--------------------------------------------------------------------------------|
     * | Type   | PacketLength | SourceUid  | DestUid  | Payload Size | Payload(Buffer) |
     * |--------------------------------------------------------------------------------|
     */
    NetworkBufferPointer toPacket() override;
    void toPacket(NetworkBufferPointer buffer);

    static std::shared_ptr<MeshDatagramPacket> toSerialize(const NetworkBufferPointer &buffer);
    static uint32_t getPayloadSize(NetworkBufferPointer buffer);
    static uint32_t getHeaderSize() { return 41; }

    std::string toString() {
        std::string result = "type: " + MeshPacket::toString(type())
                             + ", source: " + sourceUid().toString()
                             + ", dest: " + destUid().toString();
        return result;
    }

private:
    NetworkBufferPointer _payload;
};

/**
 * 변동된 그래프를 전파하기 위한 프로토콜입니다.
 */
class MeshUpdatePacket : public MeshPacket {
public:
    MeshUpdatePacket(Type type, Anyfi::UUID sourceUid, Anyfi::UUID destUid, const std::shared_ptr<GraphInfo> &graphInfo,
                     const std::shared_ptr<MeshEdge> &meshEdge, uint64_t updateTime) {
        if (type != MeshPacket::Type::GraphUpdateReq && type != MeshPacket::Type::GraphUpdateRes) {
            throw std::runtime_error("Invalid Packet Type");
        }
        _type = type;
        _sourceUid = sourceUid;
        _destUid = destUid;
        _graphInfo = graphInfo;
        _meshEdge = meshEdge;
        _updateTime = updateTime;
    }

    /**
     * Getters
     */
    uint64_t updateTime() { return _updateTime; }
    std::shared_ptr<GraphInfo> graphInfo() { return _graphInfo; }
    std::shared_ptr<MeshEdge> meshEdge() { return _meshEdge; }

    /**
     * Setters
     */
    void updateTime(uint64_t updateTime) { _updateTime = updateTime; }
    void graphInfo(const std::shared_ptr<GraphInfo> &graphInfo) { _graphInfo = graphInfo; }
    void meshEdge(const std::shared_ptr<MeshEdge> &meshEdge) { _meshEdge = meshEdge; }

    /**
     * Override : MeshPacket
     */
    /**
     * 패킷 형태
     * |-----------------------------------------------------------------------------------|
     * | 1 byte | 4 bytes      | 16 bytes   | 16 bytes | 8 Bytes    | N Bytes   | N Bytes  |
     * |-----------------------------------------------------------------------------------|
     * | Type   | PacketLength | SourceUid  | DestUid  | UpdateTime | GraphInfo | MeshEdge |
     * |-----------------------------------------------------------------------------------|
     */
    NetworkBufferPointer toPacket() override;

    static std::shared_ptr<MeshUpdatePacket> toSerialize(const NetworkBufferPointer &buffer);

    std::string toString() {
        std::string result = "type: " + MeshPacket::toString(type())
                             + ", source: " + sourceUid().toString()
                             + ", dest: " + destUid().toString()
                             + ", graphInfo: " + ((graphInfo()) ? graphInfo()->toString() : std::string("null"))
                             + ", meshEdge: " + ((meshEdge()) ? meshEdge()->toString() : "null")
                             + ", updateTime: " + to_string(updateTime());
        return result;
    }

private:
    uint64_t _updateTime = 0;
    std::shared_ptr<GraphInfo> _graphInfo;
    std::shared_ptr<MeshEdge> _meshEdge;
};

/**
 * 연결 상태를 확인하기 위한 프로토콜입니다.
 */
class MeshHeartBeatPacket : public MeshPacket {
public:
    MeshHeartBeatPacket() = default;

    MeshHeartBeatPacket(Type type, Anyfi::UUID sourceUid, Anyfi::UUID destUid) {
        _type = type;
        _sourceUid = sourceUid;
        _destUid = destUid;
    }

    /**
     * Override : MeshPacket
     */
    /**
     * 패킷 형태
     * |-----------------------------------------------|
     * | 1 byte | 4 bytes      | 16 bytes   | 16 bytes |
     * |-----------------------------------------------|
     * | Type   | PacketLength | SourceUid  | DestUid  |
     * |-----------------------------------------------|
     */
    NetworkBufferPointer toPacket() override;

    static std::shared_ptr<MeshHeartBeatPacket>
    toSerialize(const NetworkBufferPointer &buffer);

    std::string toString() {
        std::string result = "type: " + MeshPacket::toString(type())
                             + ", source: " + sourceUid().toString()
                             + ", dest: " + destUid().toString();
        return result;
    }
};

#endif //ANYFIMESH_CORE_MESHPACKET_H
