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

#ifndef ANYFIMESH_CORE_ICONNECTIVITY_H
#define ANYFIMESH_CORE_ICONNECTIVITY_H

#include <functional>
#include <vector>
#include <string>
#include <set>

#include "../Common/Network/Address.h"


/**
 * State of connectivity.
 */
enum class ConnectivityState {
    /**
     * Disconnected state of connectivity
     */
    Disconnected,

    /**
     * Connected state of connectivity
     */
    Connected,

    /**
     * Accepted state of connectivity
     */
     Accepted
};

class IConnectivity {
public:
    virtual ~IConnectivity() = default;
};


/**
 * Connectivity module interface for L4 Anyfi layer.
 */
class IConnectivityForL4 : IConnectivity {
public:
    /**
     * Get list of P2P networks that can be accepted
     *
     * @return List of P2P networks
     */
    virtual std::set<Anyfi::P2PType> getAcceptableP2P() = 0;
    /**
     * Open available P2P groups.
     *
     * @param p2PType Type of P2P group to open
     * @param callback Callback indicating address of P2P server
     */
    virtual void openP2PGroup(Anyfi::P2PType p2PType, const std::function<void(Anyfi::P2PType type, Anyfi::Address address)> &callback) = 0;
    /**
     * Called when P2P server link is opened
     *
     * @param address Address of opened P2P server link
     * @param quality network quality
     */
    virtual void onP2PServerOpened(Anyfi::Address address, int quality) = 0;

    /*
     * onP2PDisconnect
     */
    virtual void onP2PDisconnect(Anyfi::Address address) = 0;

    /**
     * Close available P2P groups.
     *
     * @param p2PType Type of P2P group to close
     */
    virtual void closeP2PGroup(Anyfi::P2PType p2PType) = 0;
    /**
     * Start or stop discovering P2P groups.
     *
     * @param discover Whether P2P group should be discovered or not
     */
    virtual void discoverP2PGroup(bool discover) = 0;

    /**
     * Disconnect P2P network of given address.
     *
     * @param address Address of P2P network to disconnect
     */
    virtual void disconnectP2P(Anyfi::Address address) = 0;

    virtual void onVPNLinkConnected(bool success) = 0;

    /*
     * ouim
     *
     * VPNTCPProcessor => Core::ouim => this::ouim
     */
    virtual void ouim(const std::string& im) = 0;
    /*
     * oudm
     *
     * VPNUDPProcessor => Core::oudm => this::oudm
     */
    virtual void oudm(const std::string& dm) = 0;

    virtual int getUidForAddr(int protocol, const std::string& srcAddr, int srcPort, const std::string& dstAddr, int dstPort) = 0;
    virtual void logMessage(int level, const std::string &message) = 0;
    virtual void setWrapperObject(const std::string& key, const std::string& value) = 0;
    virtual int detachCurrentThread() = 0;
};

#endif //ANYFIMESH_CORE_ICONNECTIVITY_H
