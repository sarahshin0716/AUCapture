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

#ifndef ANYFIMESH_CORE_L3TRANSPORTLAYER_H
#define ANYFIMESH_CORE_L3TRANSPORTLAYER_H

#include <utility>
#include <unordered_set>

#include "IL3TransportLayer.h"

#include "../L2NetworkLayer/IL2NetworkLayer.h"
#include "../L4AnyfiLayer/IL4AnyfiLayer.h"
#include "../Common/DataStructure/LRUCache.h"
#include "../L3TransportLayer/ControlBlock/VPN/VpnTCB.h"
#include "../L3TransportLayer/ControlBlock/VPN/VpnUCB.h"
#include "../L3TransportLayer/ControlBlock/Mesh/MeshEnetRUCB.h"
#include "../L3TransportLayer/ControlBlock/Mesh/MeshUCB.h"
#include "../L3TransportLayer/Processor/Vpn/TCP/VPNTCPProcessor.h"
#include "../L3TransportLayer/Processor/Vpn/UDP/VPNUDPProcessor.h"
#include "../L3TransportLayer/Processor/Proxy/ProxyPacketProcessor.h"
#include "../L3TransportLayer/Processor/Proxy/ProxyTCPProcessor.h"
#include "../L3TransportLayer/Processor/Proxy/ProxyUDPProcessor.h"
#include "../L3TransportLayer/Processor/Mesh/EnetRUDP/MeshEnetRUDPProcessor.h"
#include "../L3TransportLayer/Processor/Mesh/UDP/MeshUDPProcessor.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

#define MAX_CONTROLBLOCK_COUNT 1000


/**
 * <h1>AnyfiMesh Layer 3. Transport Layer.</h1>
 * The protocols of the transport layer provide host-to-host communication services for applications.
 * It provides services such as connection-oriented communication, reliability, flow control, and multiplexing.
 *
 * <h2>Transport Port.</h2>
 * TODO : Add comments
 *
 * <h2>Force close concept.</h2>
 * L3 Transport Layer provides a force close functionality.
 * There's two way to close session in TCP algorithm.
 * First of all, closes a session with FIN 3 way handshake. Secondly, forcibly closes a session with RST packet.
 * Similar to TCP algorithm, The L3 mesh protocol also provides force close functionality.
 */
class L3TransportLayer : public IL3TransportLayerForL2, public IL3TransportLayerForL4,
                         public L3::IL3ForPacketProcessor,
                         public std::enable_shared_from_this<L3TransportLayer> {
public:
    L3TransportLayer(Anyfi::UUID myUid);
    ~L3TransportLayer();

    /**
     * Initialize L3 Transport Layer
     */
    void initialize();
    void terminate();

    void reset();

    /**
     * Attach L2NetworkLayer Interface
     * @param l2Interface
     */
    void attachL2Interface(std::shared_ptr<IL2NetworkLayerForL3> l2Interface) { _L2 = std::move(l2Interface); }
    /**
     * Attach L4AnyfiLayer Interface
     * @param l4Interface
     */
    void attachL4Interface(std::shared_ptr<IL4AnyfiLayerForL3> l4Interface) { _L4 = std::move(l4Interface); }

    /**
     * Override interfaces : IL3TransportLayerForL2
     */
    void onReadFromMesh(Anyfi::Address srcAddr, NetworkBufferPointer buffer) override;
    void onReadFromProxy(Anyfi::Address address, NetworkBufferPointer buffer, unsigned short channelId) override;
    void onReadFromVPN(NetworkBufferPointer buffer) override;
    void onChannelClosed(unsigned short channelId, Anyfi::Address address) override;
    void onMeshNodeClosed(std::unordered_set<Anyfi::Address, Anyfi::Address::Hasher> addresses) override;

    /**
     * Override interfaces : IL3TransportLayerForL4
     */
    void openServerLink(Anyfi::Address addr, const std::function<void(Anyfi::Address address)> &callback) override;
    void closeServerLink(Anyfi::Address addr) override;
    void connectMeshLink(Anyfi::Address address, const std::function<void(Anyfi::Address address)> &callback) override;
    void closeMeshLink(Anyfi::Address address) override;
    void connectSession(Anyfi::Address address, std::function<void(unsigned short)> callback) override;
    void closeSession(unsigned short cb_id, bool force) override;
    void write(unsigned short cbId, NetworkBufferPointer buffer) override;
    void setMeshNodeType(uint8_t type) override;
    std::pair<Anyfi::Address, uint8_t> getClosestMeshNode(uint8_t type) override;
    void openExternalVPN() override;
    void openInternalVPN(int fd) override;
    void closeVPN() override;

    /**
     * Override interfaces : L3::IL3ForPacketProcessor
     */
    std::shared_ptr <ControlBlock> getControlBlock(unsigned short id) override;
    std::shared_ptr <ControlBlock> getControlBlock(ControlBlockKey &key) override;
    void onControlBlockCreated(std::shared_ptr <ControlBlock> cb) override;
    void onControlBlockDestroyed(ControlBlockKey &key) override;
    void onReadFromPacketProcessor(std::shared_ptr <ControlBlock> cb, NetworkBufferPointer buffer) override;
    void connectFromPacketProcessor(Anyfi::Address &addr, std::function<void(unsigned short channelId)> callback) override;
    bool writeFromPacketProcessor(Anyfi::Address &addr, NetworkBufferPointer buffer) override;
    void writeFromPacketProcessor(unsigned short channelId, NetworkBufferPointer buffer) override;
    void onSessionConnected(std::shared_ptr<ControlBlock> cb) override;
    void onSessionClosed(std::shared_ptr<ControlBlock> cb, bool force) override;
    uint16_t assignPort() override;
    void releasePort(uint16_t port) override;

    void dump();
private:
    // Layer interfaces
    std::shared_ptr<IL2NetworkLayerForL3> _L2;
    std::shared_ptr<IL4AnyfiLayerForL3> _L4;

    // My UID
    Anyfi::UUID _myUid;

    // ControlBlocks
    Anyfi::Config::mutex_type _cbMapMutex;
    std::unordered_map<ControlBlockKey, std::shared_ptr<ControlBlock>, ControlBlockKey::Hasher> _cbMap;
    inline void _addControlBlock(std::shared_ptr<ControlBlock> cb, ControlBlockKey key);
    inline void _removeControlBlock(ControlBlockKey &key);
    inline void _removeControlBlockWithoutLock(ControlBlockKey &key);
    inline std::shared_ptr<ControlBlock> _findControlBlock(unsigned short id);
    inline std::shared_ptr<ControlBlock> _findControlBlockWithoutLock(unsigned short id);
    inline std::shared_ptr<ControlBlock> _findControlBlock(ControlBlockKey &key);
    inline std::shared_ptr<ControlBlock> _findControlBlockWithoutLock(ControlBlockKey &key);
    inline std::list<std::shared_ptr<ControlBlock>> _findControlBlocksWithDestAddr(Anyfi::Address destAddr);
    unsigned short _getUnassignedCbId();
    ControlBlockKey *_getCBKey(unsigned short id);

    // Mesh Ports
    Anyfi::Config::mutex_type _portMutex;
    bool _ports[UINT16_MAX];
    inline uint16_t _assignUnusedPort();
    inline void _releasePort(uint16_t port);

    // Packet Processors
    std::shared_ptr<L3::VPNTCPProcessor> _VPNTCP;
    std::shared_ptr<L3::VPNUDPProcessor> _VPNUDP;
    std::shared_ptr<L3::MeshEnetRUDPProcessor> _MeshRUDP;
    std::shared_ptr<L3::MeshUDPProcessor> _MeshUDP;
    std::shared_ptr<L3::ProxyTCPProcessor> _ProxyTCP;
    std::shared_ptr<L3::ProxyUDPProcessor> _ProxyUDP;


#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(L3TransportLayer, init);
    FRIEND_TEST(L3TransportLayer, multiSession);
    FRIEND_TEST(L3TransportLayer, acceptSession);
    FRIEND_TEST(L4AnyfiLayer, ConnectAndClose);
#endif
};


#endif //ANYFIMESH_CORE_L3TRANSPORTLAYER_H
