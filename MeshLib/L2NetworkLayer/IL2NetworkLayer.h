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

#ifndef ANYFIMESH_CORE_IL2NETWORKLAYER_H
#define ANYFIMESH_CORE_IL2NETWORKLAYER_H

#include "../Common/Network/Address.h"
#include "../Common/Network/Buffer/NetworkBuffer.h"
#include "../L1LinkLayer/IL1LinkLayer.h"
#include "../L2NetworkLayer/Mesh/Neighbor.h"

class IL2NetworkLayer {
public:
    virtual ~IL2NetworkLayer() = default;
};

/**
 * L2NetworkLayer interface for L1LinkLayer
 */
class IL2NetworkLayerForL1 : public IL2NetworkLayer {
public:
    /**
     * L1에서 읽어온 패킷을 L2로 전달. L2는 VPN, Proxy, Mesh중 Link에 해당하는 곳으로 전달.
     * @param Packet buffer
     */
    virtual void onReadFromChannel(unsigned short channelId, const std::shared_ptr<Link> &link, NetworkBufferPointer buffer) = 0;

    /**
     * Link 수락 이벤트.
     * @param newlinkInfo 새로 Accepted된 링크의 정보
     */
    virtual void onChannelAccepted(const ChannelInfo &newlinkInfo) = 0;

    /**
     * Link의 상태 변화를 L2로 전달. L2는 해당 상태에 따라 Link를 정리하거나 새로 추가
     */
    virtual void onClosed(unsigned short channelId) = 0;
};

/**
 * L2NetworkLayer interface for L3TransportLayer
 */
class IL2NetworkLayerForL3 : public IL2NetworkLayer {
public:
    /**
      * External VPN Start 이벤트.
      *
      */
    virtual void openVPN() = 0;

    /**
      * Inernal VPN Start 이벤트.
      * @param fd VPN Internal FileDescriptor id
      */
    virtual void openVPN(int fd) = 0;

    /**
      * VPN Stop 이벤트.
      */
    virtual void stopVPN() = 0;

    /**
     * L3 -> L2로 패킷 송신 전달. NetworkAddress를 보고 어느 네트워크로 보내야 하는지 L3에서 판단 후 중계 전송
     * @param address
     * @param packet buffer
     */
    virtual bool write(const Anyfi::Address &address, NetworkBufferPointer buffer) = 0;
    virtual void write(unsigned short channelId, NetworkBufferPointer buffer) = 0;
    /**
     * 링크 연결 요청 메세지. L1는 링크에 unique id를 부여, Handshake 등의 과정을 거친 후 callback.
     * L2에서는 Connection Type을 확인하여 Proxy 등 L2에서 관리하는 타입일 경우, 관련 처리 후 하위 레이어로 전달.
     * @param addr 연결 요청 Anyfi::Address
     * @param callback L1 처리 끝난 후 상위 레이어로 콜백 전달. 연결 실패시 channelId = 0
     */
    virtual void connect(Anyfi::Address addr, const std::function<void(unsigned short channelId)> &callback) = 0;
    virtual void connectMesh(Anyfi::Address addr, const std::function<void(Anyfi::Address meshUid)> &callback) = 0;
    /**
     * Network Group을 생성.
     * @param networkAddr 생성 네트워크의 address
     * @return 할당받은 port number가 포함된 Address
     */
    virtual Anyfi::Address open(Anyfi::Address networkAddr) = 0;

    /**
     * 네트워크 연결 해제 이벤트.
     * @param addr 해제할 네트워크의 Anyfi::Address
     */
    virtual void close(Anyfi::Address addr) = 0;
    /**
     * 네트워크 연결 해제 이벤트.
     * @param channelId 해제할 네트워크의 channel id
     */
    virtual void close(unsigned short channelId) = 0;

    /**
     * Set my Mesh node type
     *
     * @param type Type of Mesh node to be set
     */
    virtual void setMeshNodeType(uint8_t type) = 0;

    /**
     * Get closest Mesh node of given type and its distance
     * Return NULL address if does not exist
     *
     * @param type Type of Mesh node for the closest Mesh node
     *
     * @return Pair of closest Mesh node's address and its distance
     */
    virtual std::pair<Anyfi::Address, uint8_t> getClosestMeshNode(uint8_t type) = 0;
};



#endif //ANYFIMESH_CORE_IL2NETWORKLAYER_H
