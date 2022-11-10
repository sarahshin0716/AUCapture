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

#ifndef ANYFIMESH_CORE_L2NETWORKLAYER_H
#define ANYFIMESH_CORE_L2NETWORKLAYER_H

#include "../L1LinkLayer/IL1LinkLayer.h"
#include "Mesh/MeshNetwork.h"
#include "Proxy/ProxyNetwork.h"
#include "VPN/VPNetwork.h"
#include "../L3TransportLayer/IL3TransportLayer.h"
#include "../L2NetworkLayer/Proxy/Proxy.h"
#include "IL2NetworkLayer.h"
#include "NetworkInterface.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

class L2NetworkLayer : public IL2NetworkLayerForL1,
                       public IL2NetworkLayerForL3,
                       public L2::NetworkInterfaceForMesh,
                       public L2::NetworkInterfaceForVPN,
                       public L2::NetworkInterfaceForProxy,
                       public std::enable_shared_from_this<L2NetworkLayer> {
public:
    L2NetworkLayer(Anyfi::UUID myUid, const std::string &myDeviceName);
    ~L2NetworkLayer() = default;

    /**
     * L2NetworkLayer 초기화. Mesh, Proxy, VPN 각 네트워크 Interface Attach 등 작업.
     */
    void initialize();

    /**
     * L2에서 사용되는 멤버 변수, 쓰레드 등 모두 정리
     */
    void terminate();

    /**
     * Attach L3TransportLayer Interface
     * @param l3 interface
     */
    void attachL3Interface(std::shared_ptr<IL3TransportLayerForL2> l3interface) { _L3 = std::move(l3interface); }
    /**
     * Attach L1LinkLayer Interface
     * @param l1 interface
     */
    void attachL1Interface(std::shared_ptr<IL1LinkLayerForL2> l1interface) { _L1 = std::move(l1interface); }

    /**
     * Override interfaces : IL2NetworkLayerForL1
     */
    void onReadFromChannel(unsigned short channelId, const std::shared_ptr<Link> &link, NetworkBufferPointer buffer) override;
    void onChannelAccepted(const ChannelInfo &channelInfo) override;
    void onClosed(unsigned short channelId) override;

    /**
     * Override interfaces : IL2NetworkLayerForL3
     */
    bool write(const Anyfi::Address &address, NetworkBufferPointer buffer) override;
    void write(unsigned short channelId, NetworkBufferPointer buffer) override;
    void openVPN() override;
    void openVPN(int fd) override;
    void stopVPN() override;
    void connect(Anyfi::Address addr, const std::function<void(unsigned short channelId)> &callback) override;
    void connectMesh(Anyfi::Address addr, const std::function<void(Anyfi::Address meshUid)> &callback) override;

    Anyfi::Address open(Anyfi::Address networkAddr) override;
    void close(Anyfi::Address addr) override;

    void close(unsigned short channelId) override;

    void setMeshNodeType(uint8_t type) override;
    std::pair<Anyfi::Address, uint8_t> getClosestMeshNode(uint8_t type) override;

    /**
     * Override interfaces : NetworkInterface
     */
    void writeToChannel(unsigned short channelId, NetworkBufferPointer buffer) override;
    void onChannelClosed(unsigned short channelId, Anyfi::Address addr) override;
    void connectChannel(Anyfi::Address addr, const std::function<void(unsigned short channelId)> &callback) override;

    /**
     * Override interfaces : NetworkInterfaceForMesh
     */
    /**
     * MeshHandshake에 성공할 경우 호출, chnnel id에 해당하는 callback이 _connectCallbackMap 에 존재할 경우,
     * chnnel id를 파라미터로 call
     * @param uuid MeshHandshake에 성공한 링크의 uuid
     */
    void onMeshConnected(std::shared_ptr<ChannelInfo> channelInfo, Anyfi::UUID uuid) override;
    /**
     * MeshHandshake에 실패할 경우 호출, chnnel id에 해당하는 callback이 _connectCallbackMap 에 존재할 경우,
     * -1을 파라미터로 call
     * @param channelInfo MeshHandshake에 실패한 링크의 chnnel id
     */
    void onMeshConnectFail(std::shared_ptr <ChannelInfo> channelInfo) override;
    void onReadMeshPacketFromNetwork(Anyfi::Address addr, NetworkBufferPointer buffer) override;
    void closeNeighborChannel(unsigned short channelId) override;
    void onMeshNodeClosed(std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses) override;
    /**
     * Override interfaces : NetworkInterfaceForVPN
     */
    void onReadIPPacketFromVPN(NetworkBufferPointer buffer) override;

    /**
     * Override interfaces : NetworkInterfaceForProxy
     */
    void onReadIPPacketFromProxy(Anyfi::Address addr, NetworkBufferPointer buffer, unsigned short channelId) override;

private:
    std::shared_ptr<IL3TransportLayerForL2> _L3;
    std::shared_ptr<IL1LinkLayerForL2> _L1;
    std::shared_ptr<L2::MeshNetwork> _meshNetwork;
    std::shared_ptr<L2::ProxyNetwork> _proxyNetwork;
    std::shared_ptr<L2::VPNetwork> _vPNetwork;

    /**
     * Anyfi::Address, chnnel id 매핑
     */
    std::unordered_map<Anyfi::Address, unsigned short, Anyfi::Address::Hasher> _addressLinkMap;
    Anyfi::Config::mutex_type _addressLinkMapMutex;
    void _addAddressLinkMap(std::pair<Anyfi::Address, unsigned short> addressPair);
    /**
     * _addressLinkMap 아이템 제거
     * @param addr 제거할 아이템의 Key
     * @param pair 제거한 Pair
     * @return 제거 여부. map에 key가 존재할 경우 true, 아니면 false
     */
    bool _removeAddressLinkMap(Anyfi::Address addr, std::pair<Anyfi::Address, unsigned short> *pair);

    /**
     * Anyfi::Address에 대한 Callback Map.
     * Connect 요청 시, 보유하고 있다가 L1/L2 Handshake 모두 끝난 후 Callback 호출처리
     */
    typedef std::pair<Anyfi::Address, std::function<void(Anyfi::Address meshUid)>> MeshCallbackPair;
    std::unordered_map<Anyfi::Address, std::function<void(Anyfi::Address meshUid)>, Anyfi::Address::Hasher> _connectCallbackMap;
    Anyfi::Config::mutex_type _connectCallbackMapMutex;
    void _addMeshConnectCallbackMap(Anyfi::Address addr, const std::function<void(Anyfi::Address meshUid)> &callback);
    /**
     * _connectCallbackMap 아이템 제거
     * @param addr 아이템의 Key (링크 IP주소)
     * @param pair 제거한 Pair
     * @return 제거 여부. map에 key가 존재할 경우 true, 아니면 false
     */
    bool _removeMeshConnectCallbackMap(Anyfi::Address addr, MeshCallbackPair *pair);

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(L2NetworkLayer, MeshConnect);
    FRIEND_TEST(L2NetworkLayer, MeshReadAndWrite);
    FRIEND_TEST(L2NetworkLayer, OpenAndCloseNetwork);

    FRIEND_TEST(L2NetworkLayerThread, MeshConnect);
    FRIEND_TEST(L2NetworkLayerThread, ProxyClose);
    FRIEND_TEST(L2NetworkLayerThread, VPNStartAndStop);
    FRIEND_TEST(L2NetworkLayerThread, VPNStartAndStopWithFD);
    FRIEND_TEST(L2NetworkLayerThread, VPNWrite);
    FRIEND_TEST(L2NetworkLayerThread, VPNRead);
    FRIEND_TEST(L2NetworkLayerThread, MeshWrite);
    FRIEND_TEST(L2NetworkLayerThread, MeshWriteAndRead);
    FRIEND_TEST(L4AnyfiLayer, ConnectAndClose);
#endif

};


#endif //ANYFIMESH_CORE_L2NETWORKLAYER_H
