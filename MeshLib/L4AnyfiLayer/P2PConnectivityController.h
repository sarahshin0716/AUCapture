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

#ifndef ANYFIMESH_CORE_P2PCONNECTIVITYCONTROLLER_H
#define ANYFIMESH_CORE_P2PCONNECTIVITYCONTROLLER_H

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

#include <unordered_map>
#include <set>
#include <mutex>

#include "../Common/Network/Address.h"
#include "../Common/Uuid.h"
#include "../Connectivity/IConnectivity.h"
#include "../L3TransportLayer/IL3TransportLayer.h"


namespace L4 {
struct EnumClassHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

class P2PConnectivityController {
public:
    P2PConnectivityController();
    ~P2PConnectivityController() = default;

    /**
     * Attach L3TransportLayer Interface
     * @param l3 Interface
     */
    void attachL3Interface(std::shared_ptr<IL3TransportLayerForL4> l3Interface) { _L3 = l3Interface; }
    /**
     * Attach ConnectivityLayer Interface
     * @param Connectivity Interface
     */
    void attachConnectivityInterface(std::shared_ptr<IConnectivityForL4> connectivityInterface) { _Connectivity = connectivityInterface; }

    /**
     * Start or Stop accepting P2P.
     * On start, get acceptable P2P list and open unopened P2P Group and P2P Server Link.
     * On stop, closes opened P2P Group and P2P Server Link.
     *
     * @param accept Whether P2P accepting should start or stop
     * @param quality network quality
     */
    void startAcceptP2P(bool accept, int quality = -1);

    /**
     * Connect link to given P2P address and connect session to write handshake upon success.
     * Callback address returns null if connect failed
     *
     * @param addr Address of P2P to connect to
     * @param callback Callback indicating node address of connected P2P
     */
    void connectP2P(Anyfi::Address addr, const std::function<void(Anyfi::Address addr)> &callback);

    /**
     * Accept link to given P2P address.
     * If currently accepting P2P, save accepted link information.
     * Otherwise disconnect link with given P2P
     *
     * @param addr Address of accepted P2P
     */
    void acceptP2P(Anyfi::Address addr);

    /**
     * Close links to given address and disconnect.
     * If addr is P2P address, related P2P link is closed.
     * If addr is Node address, related P2P is disconnected.
     *
     * If addr is null, close all P2P links and disconnect from all P2P
     *
     * @param addr Address of P2P to disconnect from
     */
    void disconnectP2P(Anyfi::Address addr);

    /**
     * Close accepted links to P2P and disconnect.
     */
    void disconnectAcceptedP2P();

    void discoverP2PGroup(bool discover);

protected:

// Do not override these methods.
private:
    std::shared_ptr<IL3TransportLayerForL4> _L3;
    std::shared_ptr<IConnectivityForL4> _Connectivity;

    bool _acceptP2P = false;
    int _acceptQuality = 0;

    typedef std::unordered_map<Anyfi::P2PType, Anyfi::Address, EnumClassHash> ServerAddressMap;
    typedef std::unordered_map<Anyfi::Address, Anyfi::Address, Anyfi::Address::Hasher> LinkAddressMap;
    typedef std::unordered_map<Anyfi::Address, ConnectivityState, Anyfi::Address::Hasher> LinkStateMap;
    ServerAddressMap _serverAddrMap;
    // Link Node Address to State map for both connected and accepted links
    std::unordered_map<Anyfi::Address, ConnectivityState, Anyfi::Address::Hasher> _linkStateMap;
    // Link P2P Address to Link Node Address map for connected links
    LinkAddressMap _linkAddrMap;

    ServerAddressMap _getServerAddressMap();
    ServerAddressMap _getServerAddressMapWithoutLock();
    Anyfi::Address _getServerAddress(Anyfi::P2PType type);
    Anyfi::Address _getServerAddressWithoutLock(Anyfi::P2PType type);
    void _clearServerAddressMap();
    void _clearServerAddressMapWithoutLock();

    LinkAddressMap _getLinkAddressMap();
    LinkAddressMap _getLinkAddressMapWithoutLock();
    void _insertLinkAddress(Anyfi::Address address, Anyfi::Address otherAddress);
    void _insertLinkAddressWihtoutLock(Anyfi::Address address, Anyfi::Address otherAddress);
    Anyfi::Address _getOtherLinkAddress(Anyfi::Address address);
    Anyfi::Address _getOtherLinkAddressWithoutLock(Anyfi::Address address);
    void _removeLinkAddress(Anyfi::Address address);
    void _removeLinkAddressWithoutLock(Anyfi::Address address);

    LinkStateMap _getLinkStateMap();
    LinkStateMap _getLinkStateMapWithoutLock();
    ConnectivityState _getLinkState(Anyfi::Address address);
    ConnectivityState _getLinkStateWithoutLock(Anyfi::Address address);
    void _insertLinkState(Anyfi::Address address, ConnectivityState state);
    void _insertLinkStateWihtoutLock(Anyfi::Address address, ConnectivityState state);
    void _removeLinkState(Anyfi::Address address);
    void _removeLinkStateWithoutLock(Anyfi::Address address);

    void _clearLinkStateMap();
    void _clearLinkStateMapWithoutLock();

    Anyfi::Config::mutex_type _serverAddrMutex;
    Anyfi::Config::mutex_type _linkStateMutex;
    Anyfi::Config::mutex_type _linkAddrMutex;

    Anyfi::Config::mutex_type _acceptMutex;

    /**
     * Check whether server link is opened for given p2p type
     *
     * @param type P2P type of server link to be checked
     * @return Whether related server link is opened or nor
     */
    bool _isServerOpened(Anyfi::P2PType type);
    void _onP2PGroupOpened(Anyfi::P2PType p2pType, Anyfi::Address address);

    ConnectivityState getLinkConnectionState(Anyfi::Address address);
    std::vector<Anyfi::Address> _getAcceptedLinkAddr();

    void addLink(Anyfi::Address nodeAddr, ConnectivityState connectivityState);
    void addLink(Anyfi::Address p2pAddr, Anyfi::Address nodeAddr, ConnectivityState connectivityState);

    void removeLink(Anyfi::Address address);

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
//        FRIEND_TEST(P2PConnectivityController, Init);
//        FRIEND_TEST(P2PConnectivityController, startAccept);
//        FRIEND_TEST(P2PConnectivityController, startAcceptNoP2P);
//        FRIEND_TEST(P2PConnectivityController, startAcceptOpenGroupFail);
//        FRIEND_TEST(P2PConnectivityController, startAcceptOpenServerFail);
//        FRIEND_TEST(P2PConnectivityController, stopAccept);
//        FRIEND_TEST(P2PConnectivityController, connectP2P);
//        FRIEND_TEST(P2PConnectivityController, connectP2PFail);
//        FRIEND_TEST(P2PConnectivityController, onMeshLinkAccepted);
//        FRIEND_TEST(P2PConnectivityController, disconnectP2POnP2PDisconnected);
//        FRIEND_TEST(P2PConnectivityController, disconnectP2POnMeshLinkClosed);
//        FRIEND_TEST(P2PConnectivityController, disconnectAcceptedMeshLink);
//        FRIEND_TEST(P2PConnectivityController, disconnectAcceptedP2P);
//        FRIEND_TEST(P2PConnectivityController, disconnectP2PAll);
#endif
};

}
#endif //ANYFIMESH_CORE_P2PCONNECTIVITYCONTROLLER_H
