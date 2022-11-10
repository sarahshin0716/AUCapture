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

#ifndef ANYFIMESH_CORE_MESHNETWORK_H
#define ANYFIMESH_CORE_MESHNETWORK_H

#include <unordered_map>
#include <future>
#include <thread>
#include <mutex>
#include <list>

#include "Neighbor.h"
#include "MeshPacket.h"
#include "MeshGraph.h"
#include "MeshNode.h"
#include "../../L2NetworkLayer/NetworkInterface.h"
#include "../../L2NetworkLayer/Network.h"
#include "../../Common/Network/Buffer/NetworkBuffer.h"
#include "../../Log/Backend/LogManager.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#include <Log/Backend/LogManager.h>

class L4AnyfiLayer_ConnectAndClose_Test;
#endif

namespace L2 {


typedef std::unordered_set<Anyfi::UUID, Anyfi::UUID::Hasher> UuidSet;

/**
 * MeshNetwork에서의 연결수립, 데이터 중계 등의 매니지먼트를 위한 클래스입니다.
 * 기능:
 * 1. Anyfi MeshNetwork에 맞춰 동작할 수 있도록 L2Handshake를 요청하거나 받아들입니다.
 *    Handshake를 통해 그래프 정보와 디바이스 정보를 주고받습니다.
 * 2. 다른 노드에서 받은 데이터가 자기 자신이 도착지가 아닐 경우 다시 중계합니다.
 * 3. 자기 자신을 목적지로 도착한 데이터의 경우 L4로 전달합니다.
 * 4. 일정한 주기에 맞춰 HeartbeatProtocol을 발생시켜 연결 상태를 확인합니다.
 */
class MeshNetwork : public L2::Network {
public:
    explicit MeshNetwork(Anyfi::UUID myUid, const std::string &myDeviceName);

    /**
     * MeshNetwork 정리
     */
    void clean();

    /**
     * MeshTimeoutThread 초기화 및 동작
     */
    void initTimer();
    /**
     * MeshTimeoutThread 정리
     */
    void cancelTimer();
    /**
     * Override interfaces : L2::Network
     */
    void attachL2Interface(std::shared_ptr<L2::NetworkInterface> l2interface) override {
        _L2 = std::dynamic_pointer_cast<L2::NetworkInterfaceForMesh>(l2interface);
    }
    bool isContained(unsigned short channelId) override;
    void onRead(unsigned short channelId, const std::shared_ptr<Link> &link, NetworkBufferPointer newBuffer) override;
    void write(Anyfi::Address dstAddr, NetworkBufferPointer buffer) override {};
    void onClose(unsigned short channelId) override;

    /**
     * Neighbor 연결 수동 해제
     * @param uid
     */
    void close(Anyfi::UUID uid);
    /**
     * Neighbor 연결 수동 해제
     * @param channelId
     */
    void close(unsigned short channelId);
    /**
     * Send NetworkBuffer to the node.
     * @param dst_uid destination uid
     * @param buffer NetworkBuffer without MeshHeader
     */
    bool write(Anyfi::UUID dst_uid, NetworkBufferPointer buffer);

    /**
     * Gateway 연결 완료 이벤트. Mesh Handshake 수행
     * @param linkInfo Gateway's link id
    */
    void onChannelConnected(const ChannelInfo &linkInfo);
    /**
     * 링크 연결 수락 이벤트
     * @param linkInfo 새로 연결된 링크
     */
    void onChannelAccepted(const ChannelInfo &linkInfo);

    /**
     * 지정된 시간마다 호출됩니다.
     * 각종 일정시간마다 동작해야하는 기능들을 담고있습니다.
     */
    void timeout();

    /**
     * 해당 UID와 관련된 정보 정리
     * @param uid
     */
    std::unordered_set<Anyfi::UUID, Anyfi::UUID::Hasher> clean(Anyfi::UUID uid);

    Anyfi::UUID getMyUid() { return _myUid; }

    void setMeshNodeType(uint8_t type);
    std::pair<Anyfi::Address, uint8_t> getClosestMeshNode(uint8_t type);
private:
    /**
     * channelId 매칭되는 Neighbor를 찾음.
     * @param channelId
     * @return null_ptr if not found. NeighborState otherwise.
     */
    std::shared_ptr<Neighbor> _getNeighborByLink(unsigned short channelId);
    /**
     * UID와 매칭되는 NeighborState를 찾음.
     * @param uid
     * @return null_ptr if not found. NeighborState otherwise.
     */
    std::shared_ptr<Neighbor> _getNeighborByUID(Anyfi::UUID uid);
    /**
     * NeighborState 생성 & neighborStateMap 추가
     * @param serverLink
     * @param newLink
     * @return std::shared_ptr<NeighborState> 생성된 NeighborState
     */
    std::shared_ptr<Neighbor> _addNeighbor(const ChannelInfo &channelInfo);
    /**
     * Mesh로 연결된 모든 Neighbor를 정리합니다.
     */
    void _closeAllNeighbors();

    /**
     * Send MeshProtocol to the node.
     * @param dst_uid destination uid
     * @param meshProtocol MeshProtocol
     */
    void _write(Anyfi::UUID dst_uid, std::shared_ptr<MeshPacket> meshProtocol);
    /**
     * boardcast updated mesh graph
     * @param sourceUid 정보를 전달한 Neighbor의 UID
     * @param graphInfo MeshEdge로 인하여 변경된 GraphInfo
     * @param meshEdge 새로 연결되거나 끊긴 Edge.
     */
    void _broadcastGraph(Anyfi::UUID sourceUid, std::shared_ptr<GraphInfo> graphInfo,
                         std::shared_ptr<MeshEdge> meshEdge);

    /**
     * Retransmit Handshake, GraphUpdate, Heartbeat Protocol for given neighbor
     * @param neighborState
     * @param type timeout type
     */
    void _retransmit(std::shared_ptr<Neighbor> neighbor, unsigned short timerIdx);

    void _processRead(unsigned short channelId, NetworkBufferPointer buffer);

    /**
     * Handle MeshHandshakePacket
     * @param neighborState from which packet was received
     * @param mhp MeshHandshakeProtocol Packet
     */
    void _onReadMeshHandshakePacket(unsigned short channelId, std::shared_ptr<MeshHandshakePacket> mhp);
    /**
     * Handle MeshUpdatePacket
     * @param neighborState from which packet was received
     * @param mup MeshUpdateProtocol Packet
     */
    void _onMeshUpdatePacket(std::shared_ptr<Neighbor> neighbor, std::shared_ptr<MeshUpdatePacket> mup);
    /**
     * Handle MeshHeartbeatPacket
     * @param neighborState from which packet was received
     * @param heartbeatPacket MeshHeartBeat Packet
     */
    void _onMeshHeartBeatPacket(std::shared_ptr<MeshHeartBeatPacket> heartbeatPacket);

    /**
     * Neighbor close callback
     * @param channelId neighbor's link id
     * @param isForce
     */
    void _neighborClosed(unsigned short channelId, bool isForce);
    /**
     * Neighbor timeout callback
     * @param channelId neighbor's link id
     * @param timerIdx Timer Index. {@link MeshTimer}
     */
    void _neighborTimeout(unsigned short channelId, unsigned short timerIdx);
    /**
     * Neighbor retransmission failed callback
     * @param channelId neighbor's link id
     * @param timerIdx Timer Index. {@link MeshTimer}
     */
    void _neighborRetransmissionFailed(unsigned short channelId, unsigned short timerIdx);

    void _onNeighborEstablished(std::shared_ptr<Neighbor> neighbor);

    /**
     * Neighbor Map <channelId, Neighbor>
     */
    std::unordered_map<unsigned short, std::shared_ptr<Neighbor>> _neighborMap;
    Anyfi::Config::mutex_type _neighborMapMutex;
    void _addNeighborMap(unsigned short channelId, const std::shared_ptr<Neighbor> &neighbor);
    void _removeNeighborMap(unsigned short channelId);
    std::unordered_map<unsigned short, std::shared_ptr<Neighbor>> _copyNeighborMap();

    Anyfi::UUID _myUid;
    std::string _myDeviceName;
    std::shared_ptr<MeshGraph> _meshGraph;
    uint8_t _myMeshNodeType = 0;

    std::map<unsigned short, std::list<NetworkBufferPointer>> _readBufferMap;

    /**
     * 지정된 주기마다 void timeout()을 호출하는 스레드입니다.
     */
    bool _isTimerCancel = false;
    std::unique_ptr<std::thread> _timeoutThread;
    std::shared_ptr<L2::NetworkInterfaceForMesh> _L2;

    Anyfi::LogManager *_logManager;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(MeshNetwork, NeighborAccepted);
    FRIEND_TEST(MeshNetwork, WriteMeshHandshakeReq);
    FRIEND_TEST(MeshNetwork, ReadMeshHandshakeReq);
    FRIEND_TEST(MeshNetwork, ResponseMeshHandshakeReqRetransmission);
    FRIEND_TEST(MeshNetwork, ReadMeshHandshakeReqResRetransmission);
    FRIEND_TEST(MeshNetwork, ResponseMeshHandshakeReqResRetransmission);
    FRIEND_TEST(MeshNetwork, WriteMeshHandshakeReqRetransmission);
    FRIEND_TEST(MeshNetwork, WriteMeshHandshakeResRetransmission);
    FRIEND_TEST(MeshNetwork, MeshPacketRelay);
    FRIEND_TEST(MeshNetwork, ReadMeshDatagramPacketToMe);
    FRIEND_TEST(MeshNetwork, AutoSendWriteHeartBeat);
    FRIEND_TEST(MeshNetwork, ReadHeartBeatPacket);
    FRIEND_TEST(MeshNetwork, SendHeartBeatRetransmissionFailed);
    FRIEND_TEST(MeshNetwork, MeshHandshake1GO1Client);

    FRIEND_TEST(MeshNetworkThread, MeshHandshake1GO1Client);
    FRIEND_TEST(MeshNetworkThread, MeshRetransmissionClose);
    FRIEND_TEST(MeshNetworkThread, MeshNetworkHandshake1GO2Client);
    FRIEND_TEST(MeshNetworkThread, MeshNetworkHandshake1GO2ClientAfterDisconnect);
    FRIEND_TEST(MeshNetworkThread, MeshNetwork2GO4Nodes);
    FRIEND_TEST(MeshNetworkThread, MeshNetwork6NodesWithCircle);
    FRIEND_TEST(MeshNetworkThread, DisconnectAndConenctAnotherNode);
    FRIEND_TEST(MeshNetworkThread, MeshNodeTypeUpdate);
    FRIEND_TEST(MeshNetworkThread, CloseCallback);
    friend class ::L4AnyfiLayer_ConnectAndClose_Test;
#endif

};

} // namespace L2


#endif //ANYFIMESH_CORE_MESHNETWORK_H
