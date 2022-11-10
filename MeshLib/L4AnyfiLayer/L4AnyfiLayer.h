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

#ifndef ANYFIMESH_CORE_L4ANYFILAYER_H
#define ANYFIMESH_CORE_L4ANYFILAYER_H

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

#include "../Common/fsm.hpp"
#include "../Common/Uuid.h"
#include "../Connectivity/IConnectivity.h"
#include "../L3TransportLayer/IL3TransportLayer.h"
#include "../L4AnyfiLayer/IL4AnyfiLayer.h"
#include "../L4AnyfiLayer/application/bridge/AnyfiBridgeApplication.h"
#include "../L4AnyfiLayer/P2PConnectivityController.h"
#include "../Core.h'

class L4AnyfiLayer : public IL4AnyfiLayerForL3,
                     public IL4AnyfiLayerForConnectivity,
                     public IL4AnyfiLayerForApplication,
                     public std::enable_shared_from_this<L4AnyfiLayer> {
public:
    enum NodeType : uint8_t {
        Client      = 1,
        Gateway     = 2
    };
    explicit L4AnyfiLayer(Anyfi::UUID myUid);
    ~L4AnyfiLayer() = default;

    /**
     * Attach Interfaces
     * @param L3 Interface
     */
    void attachL3Interface(std::shared_ptr<IL3TransportLayerForL4> l3Interface) {
        _L3 = std::move(l3Interface);
        _p2pConnectivityController->attachL3Interface(_L3);
    }
    /**
     * Attach ConnectivityLayer Interface
     * @param Connectivity Interface
     */
    void attachConnectivityInterface(std::shared_ptr<IConnectivityForL4> connectivityInterface) {
        _connectivity = std::move(connectivityInterface);
        _p2pConnectivityController->attachConnectivityInterface(_connectivity);
    }

    /**
     * Initialize. attach가 끝난 후 호출할 것.
     */
    void initialize();
    void terminate();
    void manage();
    Anyfi::NetworkType getTrafficType(Anyfi::Address srcAddr, Anyfi::Address destAddr);

    /**
     * Override interfaces : IL4AnyfiLayerForL3
     */
    void onSessionConnectFromVPN(unsigned short cbId, Anyfi::Address srcAddr, Anyfi::Address destAddr) override;
    void onSessionConnectFromMesh(unsigned short cbId, Anyfi::Address destAddr) override;
    void onSessionConnectFromProxy(unsigned short cbId, Anyfi::Address sourceAddr) override;
    void onReadFromSession(unsigned short cbId, NetworkBufferPointer buffer) override;
    void onSessionClose(unsigned short cbId, bool forced) override;
    void onMeshLinkClosed(Anyfi::Address address) override;
    void onMeshLinkAccepted(Anyfi::Address address) override;

    /**
     *  Override interfaces : IL4AnyfiLayerForConnectivity
     */
    void onNetworkPreferenceChanged(std::string netPref) override;
    void connectP2PLink(Anyfi::Address addr) override;
    void disconnectP2PLink(Anyfi::Address addr) override;
    void connectVPNLink(int fd) override;
    void disconnectVPNLink() override;

    /**
     * Override interfaces : IL4AnyfiLayerForApplication
     */
    void connectSession(Anyfi::Address address, std::function<void(unsigned short)> callback) override;
    void closeSession(unsigned short cbId, bool force) override;
    void write(unsigned short cbId, NetworkBufferPointer buffer) override;
    int getUidForAddr(Anyfi::Address srcAddr, Anyfi::Address destAddr) override;
    Anyfi::NetworkType getTrafficTypeForUid(int uid) override;

    void dump();

private:
    friend class Anyfi::Core;
    friend class L4AnyfiZHLayer;

    std::shared_ptr<IL3TransportLayerForL4> _L3;
    std::shared_ptr<IConnectivityForL4> _connectivity;
    std::shared_ptr<L4::P2PConnectivityController> _p2pConnectivityController;
    std::shared_ptr<L4::AnyfiBridgeApplication> _bridgeApplication;

    Anyfi::UUID _myUid = Anyfi::UUID(nullptr);
    Anyfi::Config::mutex_type _vpnMutex;
    bool _hasWiFi = false;

    std::unordered_map<int, Anyfi::NetworkType> _networkMap;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(L4ProxyApplication, VPNConnect);
    FRIEND_TEST(L4ProxyApplication, ConnectFromMesh);
    FRIEND_TEST(L4VpnToProxyTest, init);
    FRIEND_TEST(L4AnyfiLayer, ConnectAndWrite);
    FRIEND_TEST(L4AnyfiLayer, ConnectAndClose);
#endif
};

#endif //ANYFIMESH_CORE_L4ANYFILAYER_H
