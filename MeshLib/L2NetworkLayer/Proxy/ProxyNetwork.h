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

#ifndef ANYFIMESH_CORE_PROXYNETWORK_H
#define ANYFIMESH_CORE_PROXYNETWORK_H

#include <unordered_map>
#include <unordered_set>
#include <queue>

#include "../../Common/Network/Buffer/NetworkBuffer.h"
#include "../../L2NetworkLayer/NetworkInterface.h"
#include "../../L2NetworkLayer/Network.h"
#include "Proxy.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

namespace L2 {

/**
 * MeshNetwork를 통해 수신한 패킷을 외부 네트워크 중계를 위한 클래스입니다.
 */
class ProxyNetwork : public L2::Network {
public:
    ProxyNetwork();
    ~ProxyNetwork() = default;

    /**
     * Override interfaces : L2::Network
     */
    void attachL2Interface(std::shared_ptr<L2::NetworkInterface> l2interface) override {
        _L2 = std::dynamic_pointer_cast<L2::NetworkInterfaceForProxy>(l2interface);
    }
    bool isContained(unsigned short channelId) override;
    void onRead(unsigned short channelId, const std::shared_ptr<Link>& link, NetworkBufferPointer buffer) override;
    void write(Anyfi::Address dstAddr, NetworkBufferPointer buffer) override;
    void onClose(unsigned short channelId) override;

    void write(unsigned short channelId, NetworkBufferPointer buffer);
    void connect(Anyfi::Address addr, const std::function<void(unsigned short channelId)> &callback);
    /**
     * 링크 수동 해제(L3 이벤트 올라가지 않음.)
     * @param channelId
     */
    void close(unsigned short channelId);
    /**
     * 링크 상태 변화 전달 이벤트
     * @param channelId
     * @param linkState
     */
    void onLinkStateUpdated(unsigned short channelId, LinkState linkState);

private:
    std::shared_ptr<L2::NetworkInterfaceForProxy> _L2;
    /**
     * link id, Proxy 연결 Map
     */
    std::unordered_map<unsigned short, std::shared_ptr<Proxy>> _proxyMap;
    Anyfi::Config::mutex_type _proxyMapMutex;

   /**
     * Get ProxyKey from ProxyMap using destination address
     * @param dstAddr destination address
     * @return ProxyKey. 없으면 nullptr
     */
    std::shared_ptr<Proxy> _getProxy(Anyfi::Address dstAddr);
    /**
     * Get ProxyKey from ProxyMap using channelId
     * @param channelId
     * @return ProxyKey 없으면 nullptr
     */
    std::shared_ptr<Proxy> _getProxy(unsigned short channelId);
    /**
     * ProxyMap에 Proxy 추가.
     * @param channelId
     * @param dstAddr destination address
     * @return ProxyKey
     */
    std::shared_ptr<Proxy> _addProxy(unsigned short channelId, const Anyfi::Address &dstAddr);
    /**
     * ProxyMap에서 Proxy 제거
     * @param channelId 제거할 Proxy의 link id
     */
    void _removeProxy(unsigned short channelId);

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(ProxyNetwork, Init);
    FRIEND_TEST(ProxyNetwork, Connect);
    FRIEND_TEST(ProxyNetwork, ConnectWriteAndRead);
    FRIEND_TEST(ProxyNetwork, Close);
    FRIEND_TEST(ProxyNetwork, ReadFromAlreadyClosed);
    FRIEND_TEST(ProxyNetwork, LinkStateUpdate);
    FRIEND_TEST(L2NetworkLayer, Init);
#endif
};

} // namespace L2

#endif //ANYFIMESH_CORE_PROXYNETWORK_H
