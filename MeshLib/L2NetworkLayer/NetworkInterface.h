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

#ifndef ANYFIMESH_CORE_IL2NETWORKLAYERFORNETWORK_H
#define ANYFIMESH_CORE_IL2NETWORKLAYERFORNETWORK_H

#include <cstdint>
#include <unordered_set>

#include "../Common/Network/Buffer/NetworkBuffer.h"
#include "../Common/Network/Address.h"
#include "../L1LinkLayer/Link/Link.h"

// Forward declarations : L2NetworkLayer.h
class L2NetworkLayer;
/**
 * L3NetworkLayer interface for L3Network classes
 */
namespace L2 {

class NetworkInterface {
public:
    virtual ~NetworkInterface() = default;

    /**
     * 특정한 링크로 NetworkBuffer 데이터를 전송합니다.
     * @param channelId
     * @param buffer NetworkBuffer
     */
    virtual void writeToChannel(unsigned short channelId, NetworkBufferPointer buffer) = 0;

    /**
     * 링크 연결 해제 처리를 위한 메소드입니다.
     * L2NetworkLayer에서는 L3로 onNetworkClosed 이벤트를 전달합니다.
     * @param addr
     */
    virtual void onChannelClosed(unsigned short channelId, Anyfi::Address addr) = 0;

    /**
     * 링크 연결 요청 메소드입니다. L2LinkLayer::connectLink 를 호출합니다.
     * @param addr
     * @param callback
     */
    virtual void connectChannel(Anyfi::Address addr, const std::function<void(unsigned short channelId)> &callback) = 0;
};

class NetworkInterfaceForMesh : public virtual NetworkInterface {
public:
    /**
     * Mesh Network 연결 수립 이벤트
     */
    virtual void onMeshConnected(std::shared_ptr<ChannelInfo> channelInfo, Anyfi::UUID meshUuid) = 0;
    /**
     * Mesh Network 연결 실패 이벤트
     */
    virtual void onMeshConnectFail(std::shared_ptr<ChannelInfo> linkInfo) = 0;
    /**
     * Mesh Network Packet 수신 & L4로 전달
     * @param sourceAddr source address
     * @param buffer NetworkBuffer
     */
    virtual void onReadMeshPacketFromNetwork(Anyfi::Address sourceAddr, NetworkBufferPointer buffer) = 0;

    /**
     * Neigbhor가 연결 해제될 경우 호출되는 콜백입니다.
     * @param channelId 연결 해제 노드의 Address
     */
    virtual void closeNeighborChannel(unsigned short channelId) = 0;

    /**
     * Neighbor가 연결 해제될 경우 호출
     * @param uidAddress
     */
    virtual void onMeshNodeClosed(std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses) = 0;

};

class NetworkInterfaceForVPN : public virtual NetworkInterface {
public:
    /**
     * VPN Packet 수신
     * @param sourceAddr source address
     * @param buffer NetworkBuffer
     */
    virtual void onReadIPPacketFromVPN(NetworkBufferPointer buffer) = 0;
};

class NetworkInterfaceForProxy : public virtual NetworkInterface {
public:
    /**
     * Proxy Packet 수신
     * @param sourceAddr source address
     * @param buffer NetworkBuffer
     * @param channelId Channel id
     */
    virtual void onReadIPPacketFromProxy(Anyfi::Address sourceAddr, NetworkBufferPointer buffer, unsigned short channelId) = 0;
};
}
#endif //ANYFIMESH_CORE_IL2NETWORKLAYERFORNETWORK_H
