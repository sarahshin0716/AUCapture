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

#ifndef ANYFIMESH_CORE_IL4ANYFILAYER_H
#define ANYFIMESH_CORE_IL4ANYFILAYER_H

#include <functional>

#include "../Connectivity/IConnectivity.h"
#include "../Common/Network/Buffer/NetworkBuffer.h"
#include "../Common/Network/Address.h"


class IL4AnyfiLayer {
public:
    virtual ~IL4AnyfiLayer() = default;
};

/**
 * L4 anyfi layer interface for L3 transport layer
 */
class IL4AnyfiLayerForL3 : IL4AnyfiLayer {
public:
    /**
     * Callback function called when new connection is created at VPN.
     * L4AnyfiLayer creates a new mesh connection which indicates gateway node
     * and links the two.
     *
     * @param cbId The id of the control block
     * @param srcAddr The source address
     * @param destAddr The destination address
     */
    virtual void onSessionConnectFromVPN(unsigned short cbId, Anyfi::Address srcAddr, Anyfi::Address destAddr) = 0;
    /**
     * Callback function called when new connection is created at Mesh.
     * @param cbId
     * @param destAddr The dest address
     */
    virtual void onSessionConnectFromMesh(unsigned short cbId, Anyfi::Address destAddr) = 0;
    /**
     * Callback function called when new connection is created at Proxy.
     * @param cbId
     * @param sourceAddr
     */
    virtual void onSessionConnectFromProxy(unsigned short cbId, Anyfi::Address sourceAddr) = 0;

    /**
     * Callback function called when read buffer from session.
     * @param cbId
     * @param buffer
     */
    virtual void onReadFromSession(unsigned short cbId, NetworkBufferPointer buffer) = 0;
    /**
     * Callback function called when given connection is closed
     * L4Layer closes the linked connection and unlink the two.
     *
     * @param cbId The id of the control block
     * @param forced Whether close was forced or not
     */
    virtual void onSessionClose(unsigned short cbId, bool forced) = 0;

    /**
     * Callback function called when given Mesh link is closed
     * L4Layer determines whether it should close other links and
     * whether it should stop accepting
     *
     * @param address Address of closed Mesh link
     */
    virtual void onMeshLinkClosed(Anyfi::Address address) = 0;
    /**
     * Callback function called when given Mesh link is accepted
     * L4Layer updates its link information
     *
     * @param address Address of accepted Mesh link
     */
    virtual void onMeshLinkAccepted(Anyfi::Address address) = 0;
};

/**
 * L4 anyfi layer interface for connectivity manager layer
 */
class IL4AnyfiLayerForConnectivity : IL4AnyfiLayer {
public:
    /**
     * Callback function called when network preference changes
     *
     * @param netPref Network preference in Json String
     */
    virtual void onNetworkPreferenceChanged(std::string netPref) = 0;

    /**
     * Connect to P2P link with given address
     *
     * @param addr Address of P2P link
     */
    virtual void connectP2PLink(Anyfi::Address addr) = 0;

    /**
     * Disconnect to P2P link with given address
     *
     * @param addr Address of P2P link
     */
    virtual void disconnectP2PLink(Anyfi::Address addr) = 0;

    /**
     * Connect to VPN link with given file descriptor
     *
     * @param fd File descriptor of VPN link
     */
    virtual void connectVPNLink(int fd) = 0;

    /**
     * Disconnect to P2P link with given address
     *
     */
    virtual void disconnectVPNLink() = 0;
};

class IL4AnyfiLayerForApplication : IL4AnyfiLayer {
public:
    virtual void connectSession(Anyfi::Address address, std::function<void(unsigned short)> callback) = 0;
    virtual void closeSession(unsigned short cbId, bool force) = 0;
    virtual void write(unsigned short cbId, NetworkBufferPointer buffer) = 0;
    virtual int getUidForAddr(Anyfi::Address srcAddr, Anyfi::Address destAddr) = 0;
    virtual Anyfi::NetworkType getTrafficTypeForUid(int uid) = 0;
};

#endif //ANYFIMESH_CORE_IL4ANYFILAYER_H
