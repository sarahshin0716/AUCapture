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

 #ifndef ANYFIMESH_CORE_MESHENETRUDPPROCESSOR_H
#define ANYFIMESH_CORE_MESHENETRUDPPROCESSOR_H

#include "../../../../Common/Timer.h"
#include "../../../../L3TransportLayer/Processor/Mesh/MeshPacketProcessor.h"
#include "../../../../L3TransportLayer/ControlBlock/Mesh/MeshEnetRUCB.h"


/**
 * Interval of enet service timer.
 * TODO : 100ms로 increase
 */
#define ENET_TIMER_INTERVAL 10


namespace L3 {

/**
 * MeshRUDPProcessor
 */
class MeshEnetRUDPProcessor : public MeshPacketProcessor {
public:
    MeshEnetRUDPProcessor(std::shared_ptr<IL3ForPacketProcessor> l3, Anyfi::UUID myUid);
    virtual ~MeshEnetRUDPProcessor();

    static std::shared_ptr<MeshEnetRUDPProcessor> init(std::shared_ptr<IL3ForPacketProcessor> l3, Anyfi::UUID myUid);
    static std::shared_ptr<MeshEnetRUDPProcessor> getInstance();

    typedef std::function<void(std::shared_ptr<ControlBlock> cb)> ConnectCallback;
    void connect(Anyfi::Address address, ConnectCallback callback) override;
    void read(NetworkBufferPointer buffer, Anyfi::Address src) override;
    void write(NetworkBufferPointer buffer, ControlBlockKey key) override;
    void close(ControlBlockKey key, bool force) override;

    void shutdown() override;
    void reset() override;

//
// ========================================================================================
// Enet API : call from enet
//
    int enetAPISend(ENetSocket socket, const ENetAddress *address, const ENetBuffer *buffers, size_t bufferCount);
    int enetAPIRecv(ENetSocket socket, ENetAddress *address, ENetBuffer *buffers, size_t bufferCount);
//
// Enet API : call from enet
// ========================================================================================
//

protected:
    std::atomic_bool shouldShutdown;

    /**
     * Enet timer tick method.
     * It called every ${ENET_TIMER_INTERVAL} ms.
     */
    void _onEnetTimerTick();
    void enetThreadProc();

    /**
     * Do enet host service.
     */
    void _doEnetService(std::shared_ptr<MeshEnetRUCB> rucb);

    enum PendingOperation : uint8_t {
        PENDING_OPS_NOTIFY_CONNECT_SUCCESS  = 0b00000001,
        PENDING_OPS_NOTIFY_CONNECT_FAIL     = 0b00000010,
        PENDING_OPS_NOTIFY_ACCEPTED         = 0b00000100,
        PENDING_OPS_NOTIFY_READ             = 0b00001000,
        PENDING_OPS_NOTIFY_CLOSED           = 0b00010000,
    };
    /**
     * Callback function called when enet socket has been connected.
     */
    uint8_t _onEnetConnected(std::shared_ptr<MeshEnetRUCB> rucb, ENetEvent &event);
    /**
     * Callback function called when enet socket has been disconnected.
     */
    uint8_t _onEnetDisconnected(std::shared_ptr<MeshEnetRUCB> rucb, ENetEvent &event);
    /**
     * Callback function called when receive data from enet socket.
     */
    std::pair<uint8_t, NetworkBufferPointer> _onEnetReceived(std::shared_ptr<MeshEnetRUCB> rucb, ENetEvent &event);

private:
    // Singleton instance
    static std::shared_ptr<MeshEnetRUDPProcessor> _instance;

    // Enet service timer task
    std::shared_ptr<Anyfi::TimerTask> _enetTimerTask;
    std::unique_ptr<std::thread> _enetThread;

    // RUCB
    Anyfi::Config::mutex_type _rucbMapMutex;
    std::unordered_map<ControlBlockKey, std::shared_ptr<MeshEnetRUCB>, ControlBlockKey::Hasher> _rucbMap;
    void _addRUCB(const std::shared_ptr<MeshEnetRUCB> &rucb, ControlBlockKey &key);
    inline void _addRUCB(const std::shared_ptr<MeshEnetRUCB> &rucb, ControlBlockKey &&key) { _addRUCB(rucb, key); }
    void _removeRUCB(ControlBlockKey &key);
    inline void _removeRUCB(ControlBlockKey &&key) { _removeRUCB(key); }
    std::shared_ptr<L3::MeshEnetRUCB> _findRUCB(ControlBlockKey &key);
    inline std::shared_ptr<L3::MeshEnetRUCB> _findRUCB(ControlBlockKey &&key) { return _findRUCB(key); }
    std::shared_ptr<L3::MeshEnetRUCB> _findRUCB(ENetSocket sock);
    std::unordered_map<ControlBlockKey, std::shared_ptr<L3::MeshEnetRUCB>, ControlBlockKey::Hasher> _copyRUCBs();

    // Connect callback
    Anyfi::Config::mutex_type _connectCallbackMutex;
    std::unordered_map<uint16_t, ConnectCallback> _connectCallbacks;
    void _keepConnectCallback(uint16_t localPort, ConnectCallback callback);
    void _notifyConnectCallback(uint16_t localPort, std::shared_ptr<ControlBlock> resultCB);

    /**
     * Writes all pendeing write buffers.
     */
    void _writeAllPendingBuffers(std::shared_ptr<MeshEnetRUCB> rucb);
    /**
     * Writes a given packet to enet socket.
     */
    void _writeToEnet(const std::shared_ptr<MeshEnetRUCB> &rucb, NetworkBufferPointer buffer);

    /**
     * Cloeses the enet peer.
     */
    void _closeEnetPeer(std::shared_ptr<MeshEnetRUCB> rucb);
};

}   // namespace L3


#endif //ANYFIMESH_CORE_MESHENETRUDPPROCESSOR_H
