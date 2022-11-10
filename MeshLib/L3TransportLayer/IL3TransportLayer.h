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

#ifndef ANYFIMESH_CORE_IL4TRANSPORTLAYER_H
#define ANYFIMESH_CORE_IL4TRANSPORTLAYER_H

#include <unordered_set>
#include <utility>
#include "ControlBlock/Mesh/MeshControlBlock.h"

#include "../Common/Uuid.h"
#include "../Common/Network/Address.h"
#include "../Common/Network/Buffer/NetworkBuffer.h"


/**
 * Predetermined Transport Port Numbers.
 * The lowest numbered 100 port numbers are predetermined transport port numbers,
 * and identify the services running on the Application Layer.
 */
#define PORT_NUM_NOT_USED 0
#define PORT_NUM_ANYFI 1
#define PORT_NUM_ICMP 2
#define PORT_NUM_MAX_DEFINED_PORT 100
#define PORT_NUM_DNS_PORT 53


// Forward declarations : ControlBlock.h
class ControlBlock;
class ControlBlockKey;


class IL3TransportLayer {
public:
    virtual ~IL3TransportLayer() = default;
};

/**
 * L3TransportLayer interface for NetworkLayer
 */
class IL3TransportLayerForL2 : IL3TransportLayer {
public:
    /**
     * Callback function called when L2 reads a packet from the Mesh network.
     *
     * @param address The address that points the source address.
     * @param buffer A buffer which store the read message.
     */
    virtual void onReadFromMesh(Anyfi::Address address, NetworkBufferPointer buffer) = 0;
    /**
     * Callback function called when L2 reads a packet from the Proxy network.
     *
     * @param address The address that points the source address.
     * @param buffer A buffer which store the read message.
     */
    virtual void onReadFromProxy(Anyfi::Address address, NetworkBufferPointer buffer, unsigned short channelId) = 0;
    /**
     * Callback function called when L2 reads a packet from the VPN network.
     *
     * @param buffer A buffer which store the read message.
     */
    virtual void onReadFromVPN(NetworkBufferPointer buffer) = 0;

    /**
     * Callback function called when mesh neighbor link closed.
     * It only notify a close event of neighbor mesh link.
     * This method does not called when close manually. (Close mesh link with L2#close())
     *
     * @param address Address of the closed neighbor link.
     */
    virtual void onChannelClosed(unsigned short channelId, Anyfi::Address address) = 0;

    /**
     * Callback function called when mesh node closed.
     * This method does not notify a close event of neighbor mesh link.
     *
     * @param addresses The address of closed mesh nodes
     */
    virtual void onMeshNodeClosed(std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses) = 0;
};

/**
 * L3TransportLayer interface for L4 anyfi layer.
 */
class IL3TransportLayerForL4 : IL3TransportLayer {
public:
    /**
     * Open server link on given address.
     *
     * @param addr The address on which server link will be opened
     * @param callback Callback indicating address of opened server link
     */
    virtual void openServerLink(Anyfi::Address addr, const std::function<void(Anyfi::Address address)> &callback) = 0;

    /**
     * Close server link on given address.
     *
     * @param addr The address of the server link to be closed
     */
    virtual void closeServerLink(Anyfi::Address addr) = 0;

    /**
     * Connect a link to given address.
     *
     * @param address The address to which this link is to be connected.
     * @param callback Called when connect-operation is ended.
     * uid is 0 if connect-operation is failed, uid of the remote node if success.
     */
    virtual void connectMeshLink(Anyfi::Address address, const std::function<void(Anyfi::Address address)> &callback) = 0;

    /**
    * Close the link to given address.
    *
    * @param address the address to which this link is to be closed.
    */
    virtual void closeMeshLink(Anyfi::Address address) = 0;

    /**
     * Connect a session.
     *
     * @param address The address of mesh node.
     * @param callback It called when connect-operation is finished.
     * cb is 0, if connect failed, control block id otherwise.
     */
    virtual void connectSession(Anyfi::Address address, std::function<void(unsigned short)> callback) = 0;

    /**
     * Close a session.
     *
     * @param cb_id The id of session to close.
     * @param force Whether session should be force closed or not
     */
    virtual void closeSession(unsigned short cb_id, bool force) = 0;

    /**
     * Writes a given buffer to the session(cb)
     *
     * @param cb_id An id of the control block.
     * @param buffer A buffer to write.
     */
    virtual void write(unsigned short cb_id, NetworkBufferPointer buffer) = 0;

//
// ========================================================================================
// bridge interfaces which connecting L4AnyfiLayer and L2NetworkLayer
//
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
    /**
     * Open the external vpn connection.
     */
    virtual void openExternalVPN() = 0;
    /**
     * Open the internal vpn connection
     *
     * @param fd VPN file descriptor
     */
    virtual void openInternalVPN(int fd) = 0;
    /**
     * Stop the VPN connection.
     */
    virtual void closeVPN() = 0;
//
// bridge interfaces which connecting L4AnyfiLayer and L2NetworkLayer
// ========================================================================================
//
};


namespace L3 {

/**
 * L3TransportLayer interface for packet processors.
 */
class IL3ForPacketProcessor : public IL3TransportLayer {
public:
    /**
     * Gets a matching control block for the given key.
     * TODO : remove these methods.
     */
    virtual std::shared_ptr<ControlBlock> getControlBlock(unsigned short id) = 0;
    virtual std::shared_ptr<ControlBlock> getControlBlock(ControlBlockKey &key) = 0;

    /**
     * Callback function called when packet processor creates a control block.
     * L3 adds the given control block to cbMap.
     *
     * @param cb New control block created by packet processor.
     */
    virtual void onControlBlockCreated(std::shared_ptr<ControlBlock> cb) = 0;
    /**
     * Callback function called when packet processor destroy a control block.
     * L3 removes the given control block from cbMap.
     *
     * @param key The key of the destroyed control block.
     */
    virtual void onControlBlockDestroyed(ControlBlockKey &key) = 0;

    /**
     * Callback function called when session is connected.
     */
    virtual void onSessionConnected(std::shared_ptr<ControlBlock> cb) = 0;
    /**
     * Callback function callend when session is closed.
     * It's not called when closed by calling the close function.
     *
     * @param force true if forcibly close, false otherwise.
     */
    virtual void onSessionClosed(std::shared_ptr<ControlBlock> cb, bool force) = 0;

    /**
     * Callback function called when packet processor reads a packet.
     *
     * @param cb Control block.
     * @param buffer A payload of the packet.
     */
    virtual void onReadFromPacketProcessor(std::shared_ptr<ControlBlock> cb, NetworkBufferPointer buffer) = 0;

    /**
     * Connect a link from the packet processor.
     *
     * @param addr Destination address.
     * @param callback Callback function.
     */
    virtual void connectFromPacketProcessor(Anyfi::Address &addr, std::function<void(unsigned short channelId)> callback) = 0;
    /**
     * Writes a packet to lower-layer from the packet processor.
     *
     * @param cb Control block.
     * @param buffer A payload of the packet.
     */
    virtual bool writeFromPacketProcessor(Anyfi::Address &addr, NetworkBufferPointer buffer) = 0;
    virtual void writeFromPacketProcessor(Anyfi::Address &&addr, NetworkBufferPointer buffer) {
        writeFromPacketProcessor(addr, std::move(buffer));
    }
    virtual void writeFromPacketProcessor(unsigned short channelId, NetworkBufferPointer buffer) = 0;

    /**
     * Assign source port to the Mesh Control Block.
     */
    virtual uint16_t assignPort() = 0;
    /**
     * Release source port of the Mesh Control Block.
     */
    virtual void releasePort(uint16_t port) = 0;
};

}


#endif //ANYFIMESH_CORE_IL4TRANSPORTLAYER_H
