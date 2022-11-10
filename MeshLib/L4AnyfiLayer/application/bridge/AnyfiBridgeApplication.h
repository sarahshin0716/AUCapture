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

#ifndef ANYFIMESH_CORE_ANYFIPROXYAPPLICATION_H
#define ANYFIMESH_CORE_ANYFIPROXYAPPLICATION_H

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>

class L4AnyfiLayer_ConnectAndClose_Test;
#endif

#include <unordered_map>

#include "../../../Common/Network/Address.h"
#include "../../../L3TransportLayer/IL3TransportLayer.h"
#include "../../../L4AnyfiLayer/application/AnyfiApplication.h"
#include "../../../L4AnyfiLayer/application/bridge/AnyfiBridgeMap.h"

// Forward declarations : L4AnyfiLayer
class L4AnyfiLayer;
namespace L4 {
/**
 * VPN <-> Mesh : AnyfiVpnBridge
 * Mesh <-> Proxy : AnyfiProxyBridge
 */
class AnyfiBridgeApplication : public AnyfiApplication {
public:
    AnyfiBridgeApplication() = default;
    ~AnyfiBridgeApplication() = default;

    virtual void onSessionConnectFromVPN(unsigned short vpnCbId, std::vector<Anyfi::Address> gatewayList, bool useMain, Anyfi::Address srcAddr, Anyfi::Address destAddr);
    void onSessionConnectFromMesh(unsigned short cbId, std::vector<Anyfi::Address> gatewayList, bool useMain);
    void onSessionConnectFromProxy(unsigned short cbId, Anyfi::Address sourceAddr);
    virtual void onSessionClose(unsigned short cbId, bool forced);

    virtual void onReadFromSession(unsigned short cbId, const NetworkBufferPointer &buffer);

    virtual void resetSession(std::unordered_map<int, int> srcPortUidMap);
    virtual void dump();
private:
    virtual void _onReadFromVPNSession(unsigned short cbId, const NetworkBufferPointer &buffer);
    void _onReadFromMeshSession(unsigned short cbId, NetworkBufferPointer buffer);
    virtual void _onReadFromProxySession(unsigned short cbId, NetworkBufferPointer buffer);
    bool _addressOptionValidCheck(Anyfi::ConnectionType type, const Anyfi::Address &addr);

    void _processMeshConnect(std::shared_ptr<AnyfiBridge> bridge, const NetworkBufferPointer &buffer);
    void _processMeshRead(std::shared_ptr<AnyfiBridge> bridge, const NetworkBufferPointer &buffer);

    void _onSessionCloseFromProxy(unsigned short cbId, bool forced);
    void _onSessionCloseFromVPN(unsigned short cbId, bool forced);
    void _onSessionCloseFromMesh(unsigned short cbId, bool forced);

    AnyfiBridgeMap<std::vector<std::shared_ptr<L4::AnyfiBridge>>> _bridgeMap;

protected:
    void _handleSessionClosedByDest(std::shared_ptr<AnyfiBridge> bridge, bool forced);
    void _closeNonMainBridges(unsigned short cbId, bool eraseFromList);
    bool _hasMain(unsigned short cbId);
    int _removeBridge(unsigned short cbId, Anyfi::NetworkType networkType);
};
}


#endif //ANYFIMESH_CORE_ANYFIPROXYAPPLICATION_H
