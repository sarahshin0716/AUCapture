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

#ifndef ANYFIMESH_CORE_VPNTCPPROCESSOR_H
#define ANYFIMESH_CORE_VPNTCPPROCESSOR_H

#include "../../../../L3TransportLayer/Processor/Vpn/VPNPacketProcessor.h"
#include "../../../../Common/Timer.h"

#include <thread>
#include <mutex>
#include <utility>

/* Actual DNS Server: 8.8.8.8 */
#define VPN_DNS_SERVER_IPV4 0x8080808
/* Private DNS Tunnel: 10.0.0.2 */
#define VPN_DNS_TUNNEL_IPV4 0xa000002
/* Private Network: 10.0.0.1 */
#define VPN_ADDRESS_IPV4 0xa000001

// Forward declaration
class TCPHeader;
class TCPPacket;
class VpnTCB;

namespace L3 {

class VPNTCPProcessor : public VPNPacketProcessor {
public:
    explicit VPNTCPProcessor(std::shared_ptr<IL3ForPacketProcessor> l3);
    ~VPNTCPProcessor();

    void input(std::shared_ptr<IPPacket> packet) override;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override;
    void close(ControlBlockKey key, bool force) override;

    void shutdown() override;
    void clear();

private:
    Anyfi::Config::mutex_type _tcbMapMutex;
    std::unordered_map<ControlBlockKey, std::shared_ptr<VpnTCB>, ControlBlockKey::Hasher> _tcbMap;
    void _addTCB(const std::shared_ptr<VpnTCB> &tcb, ControlBlockKey &key);
    inline void _addTCB(const std::shared_ptr<VpnTCB> &tcb, ControlBlockKey &&key) { _addTCB(tcb, key); }
    void _removeTCB(ControlBlockKey &key);
    inline void _removeTCB(ControlBlockKey &&key) { _removeTCB(key); }
    std::shared_ptr<VpnTCB> _findTCB(ControlBlockKey &key);
    inline std::shared_ptr<VpnTCB> _findTCB(ControlBlockKey &&key) { return _findTCB(key); }

    /**
     * TCP timer tick method.
     * It called every 100 ms.
     */
    void _tcpTimerTick();
    std::shared_ptr<Anyfi::TimerTask> _timerTask;

    enum ConnectionStateChange : uint8_t {
        NONE = 0,
        CONNECTED = 1,
        CLOSED = 2,
        RESET = 3,
    };
    typedef std::pair<ConnectionStateChange, NetworkBufferPointer> ProcessResult;
    std::shared_ptr<VpnTCB> _processSYN(TCPHeader *tcpHeader, ControlBlockKey &key);
    ProcessResult _processDuplicateSYN(TCPHeader *tcpHeader, std::shared_ptr<VpnTCB> tcb);
    ProcessResult _processRST(TCPHeader *tcpHeader, std::shared_ptr<VpnTCB> tcb);
    ProcessResult _processData(TCPPacket *tcpPacket, std::shared_ptr<VpnTCB> tcb);
    ProcessResult _processACK(TCPPacket *tcpPacket, std::shared_ptr<VpnTCB> tcb);

    /**
     * Close function implementation without lock.
     * Call this function after lock the given tcb.
     */
    void _closeImplWithoutLock(std::shared_ptr<VpnTCB> tcb, bool force);

    /**
     * Reads a payload in the tcp packet.
     * It updates TCB, and pass payload to the L3.
     */
    NetworkBufferPointer _readPayload(TCPPacket *tcpPacket, std::shared_ptr<VpnTCB> tcb);

    /**
     * Check packet ordering is valid.
     * If receive out-of-order packet, sends a duplicate ack.
     * If receive duplicate ack packet, do retransmit.
     *
     * @return true if valid, false otherwise.
     */
    bool _validateOrdering(TCPHeader *tcpHeader, const std::shared_ptr<VpnTCB> &tcb);


    /**
     * Sends an RST to the packet received.
     */
    void _doSimpleRST(std::shared_ptr<VpnTCB> tcb);

    /**
     * Sends an ACK to the packets received so far (No data included).
     */
    void _doSimpleACK(std::shared_ptr<VpnTCB> tcb);

    /**
     * Sends an ACK to the packets received so far.
     */
    void _doACK(std::shared_ptr<VpnTCB> tcb);

    /**
     * Do retransmit.
     */
    void _retransmit(std::shared_ptr<VpnTCB> tcb);

    /**
     * Sends a FIN packet.
     */
    void _sendFIN(std::shared_ptr<VpnTCB> tcb);
};

}   // namespace L3


#endif //ANYFIMESH_CORE_VPNTCPPROCESSOR_H
