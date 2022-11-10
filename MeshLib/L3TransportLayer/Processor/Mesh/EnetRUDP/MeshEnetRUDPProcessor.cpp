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

 #include <L3TransportLayer/Processor/Mesh/EnetRUDP/enet/unix.h>
#include <Core.h>
#include "MeshEnetRUDPProcessor.h"

#include "L3TransportLayer/Packet/TCPIP_L4_Application/MeshEnetRUDPHeader.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"


#include "enet/enet.h"
#include "enet/unix.h"


//
// ========================================================================================
// MeshEnetRUDPProcessor singleton impl
//
std::shared_ptr<L3::MeshEnetRUDPProcessor> L3::MeshEnetRUDPProcessor::_instance = nullptr;

L3::MeshEnetRUDPProcessor::MeshEnetRUDPProcessor(std::shared_ptr<IL3ForPacketProcessor> l3,
                                                 Anyfi::UUID myUid)
        : MeshPacketProcessor(l3, myUid) {
    if (enet_initialize() != 0) {
        throw std::runtime_error("Failed to initialize enet");
    }

//    _enetTimerTask = Anyfi::Timer::schedule(
//            std::bind(&L3::MeshEnetRUDPProcessor::_onEnetTimerTick, this),
//            ENET_TIMER_INTERVAL, ENET_TIMER_INTERVAL);
    shouldShutdown = false;
    _enetThread.reset(new std::thread(std::bind(&L3::MeshEnetRUDPProcessor::enetThreadProc, this)));
}

L3::MeshEnetRUDPProcessor::~MeshEnetRUDPProcessor() {
    shutdown();
}

std::shared_ptr<L3::MeshEnetRUDPProcessor>
L3::MeshEnetRUDPProcessor::init(std::shared_ptr<L3::IL3ForPacketProcessor> l3, Anyfi::UUID myUid) {
    _instance = std::make_shared<MeshEnetRUDPProcessor>(l3, myUid);
    return _instance;
}

std::shared_ptr<L3::MeshEnetRUDPProcessor> L3::MeshEnetRUDPProcessor::getInstance() {
    if (_instance == nullptr)
        throw std::runtime_error("MeshEnetRUDPProcessor instance is nullptr");

    return _instance;
}
//
// MeshEnetRUDPProcessor singleton impl
// ========================================================================================
//


//
// ========================================================================================
// MeshEnetRUDPProcessor public methods
//
void L3::MeshEnetRUDPProcessor::connect(Anyfi::Address address,
                                        L3::MeshEnetRUDPProcessor::ConnectCallback callback) {
    if (address.type() != Anyfi::AddrType::MESH_UID)
        throw std::invalid_argument("Destination address type should be MESH_UID");
    if (address.connectionProtocol() != Anyfi::ConnectionProtocol::MeshRUDP)
        throw std::invalid_argument("Connection protocol must be MeshUDP");

    Anyfi::Address src;
    src.setMeshUID(_myUid);
    src.port(_L3->assignPort());
    src.connectionType(Anyfi::ConnectionType::WifiP2P); //TODO: General P2P Type
    src.connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);

    ControlBlockKey key(src, address);
    auto rucb = std::make_shared<MeshEnetRUCB>(key);

    // Create enet host
    ENetHost *host;
    host = enet_host_create(nullptr, 1, 2, 0, 0);
    host->socket = src.port();
    rucb->host(host);

    // Enet Connect
    ENetAddress peerAddr{};
    enet_address_set_host(&peerAddr, "0.0.0.0");
    peerAddr.port = address.port();
    peerAddr.ext = rucb.get();
    ENetPeer *peer = enet_host_connect(host, &peerAddr, 0, 0);
    rucb->peer(peer);

    // Keep connect callback
    _keepConnectCallback(src.port(), callback);

    // Add connecting control block.
    // This operation should be last. (타이머에서 먼저 처리되는것을 방지하기 위함)
    rucb->state(MeshEnetRUCB::State::CONNECTING);
    _addRUCB(rucb, key);
}

void L3::MeshEnetRUDPProcessor::read(NetworkBufferPointer buffer, Anyfi::Address src) {
    if (buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be forward networkMode.");

    auto header = std::make_shared<MeshEnetRUDPHeader>(buffer, buffer->getReadPos());
    src.port(header->sourcePort());
    src.connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);

    Anyfi::Address dst;
    dst.port(header->destPort());
    dst.setMeshUID(_myUid);
    dst.connectionType(Anyfi::ConnectionType::WifiP2P); //TODO: General P2P Type
    dst.connectionProtocol(Anyfi::ConnectionProtocol::MeshRUDP);

    ControlBlockKey key(dst, src);
    auto rucb = _findRUCB(key);
    if (rucb == nullptr) {
        // From unknown session
        rucb = std::make_shared<MeshEnetRUCB>(key);

        // Add new ACCEPT_WAIT rucb
        rucb->state(MeshEnetRUCB::State::ACCEPT_WAIT);
        _addRUCB(rucb, key);

        /**
         * Create host
         */
        ENetHost *host = enet_host_create(nullptr, 1, 2, 0, 0);
        host->socket = rucb->id();   // TODO : id 및 port 개념 정리
        rucb->host(host);
    }

    // Put read buffer
    buffer->setReadPos(buffer->getReadPos() + header->length());
    rucb->putReadBuffer(buffer);

    // Do enet service
    _doEnetService(rucb);
}

void L3::MeshEnetRUDPProcessor::write(NetworkBufferPointer buffer, ControlBlockKey key) {
    if (buffer == nullptr)
        throw std::invalid_argument("Buffer should not be nullptr");
    else if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    auto rucb = _findRUCB(key);
    if (rucb == nullptr) {
        Anyfi::Log::error(__func__, "RUCB is nullptr");
        return;
    }

    {
        Anyfi::Config::lock_type guard(rucb->_mutex);
        if (rucb->state() == MeshEnetRUCB::State::CONNECTING) {
            // If enet socket is connecting, learn to write queue and wait until connected.
            rucb->putWriteBuffer(buffer);
            return;
        } else if (rucb->state() == MeshEnetRUCB::State::CONNECTED) {
            _writeToEnet(rucb, buffer);
        } else {
            Anyfi::Log::error(__func__, "Invalid state");
            return;
        }
    }

    _doEnetService(rucb);
}

void L3::MeshEnetRUDPProcessor::close(ControlBlockKey key, bool force) {
    auto rucb = _findRUCB(key);
    if (rucb == nullptr) {
        Anyfi::Log::error(__func__, "rucb is nullptr");
        return;
    }

    Anyfi::Config::lock_type guard(rucb->_mutex);

    if (rucb->state() <= MeshEnetRUCB::State::CONNECTED) {
        if (force) {
            _closeEnetPeer(rucb);
        } else {
            rucb->state(MeshEnetRUCB::State::CLOSE_WAIT);
        }
    }
}

void L3::MeshEnetRUDPProcessor::shutdown() {
    if (_enetTimerTask != nullptr) {
        Anyfi::Timer::cancel(_enetTimerTask);
        _enetTimerTask = nullptr;
    }

    shouldShutdown = true;
    if(_enetThread != nullptr) {
        _enetThread->join();
        _enetThread = nullptr;
    }

    enet_deinitialize();
}

void L3::MeshEnetRUDPProcessor::reset() {
    // FIXME:
    Anyfi::Config::lock_type guard(_rucbMapMutex);

    // Close all rucb
    for (auto it : _rucbMap) {
        std::shared_ptr<L3::MeshEnetRUCB> rucb = it.second;

        rucb->state(MeshEnetRUCB::State::CLOSED);

        auto key = rucb->key();
        _L3->releasePort(key.srcAddress().port());
        _L3->onControlBlockDestroyed(key);
        _L3->onSessionClosed(rucb, true);

        auto host = rucb->host();
        if (host != nullptr)
            enet_host_destroy(host);
    }
    _rucbMap.clear();
}

//
// MeshEnetRUDPProcessor public methods
// ========================================================================================
//


//
// ========================================================================================
// Enet API : call from enet
//
int L3::MeshEnetRUDPProcessor::enetAPISend(ENetSocket socket, const ENetAddress *address,
                                           const ENetBuffer *buffers,
                                           size_t bufferCount) {
    auto rucb = _findRUCB(socket);
    if (rucb == nullptr) {
        Anyfi::Log::error(__func__, "rucb is nullptr");
        return -1;
    }

    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();

    int sent = 0;
    for (int i = bufferCount - 1; i >= 0; i--) {
        const ENetBuffer *enetBuffer = (buffers + i);
        buffer->putBytes((uint8_t *) enetBuffer->data, (size_t) enetBuffer->dataLength);
        sent += enetBuffer->dataLength;
    }
    MeshEnetRUDPHeaderBuilder builder;
    builder.sourcePort(rucb->key().srcAddress().port());
    builder.destPort(rucb->key().dstAddress().port());
    builder.build(buffer, buffer->getWritePos());

    _L3->writeFromPacketProcessor(rucb->key().dstAddress(), buffer);

    return sent;
}

int
L3::MeshEnetRUDPProcessor::enetAPIRecv(ENetSocket socket, ENetAddress *address, ENetBuffer *buffers,
                                       size_t bufferCount) {
    auto rucb = _findRUCB(socket);
    if (rucb == nullptr) {
        Anyfi::Log::error(__func__, "rucb is nullptr");
        return -1;
    }

    auto readBuffer = rucb->popReadBuffer();
    if (readBuffer == nullptr)
        return 0;

    struct iovec iov = ((struct iovec *) buffers)[0];
    auto buf = (uint8_t *) iov.iov_base;

    int len = readBuffer->getWritePos() - readBuffer->getReadPos();
    readBuffer->getBytes(buf, len);
    return len;
}
//
// Enet API : call from enet
// ========================================================================================
//


//
// ========================================================================================
// MeshEnetRUDPProcessor protected methods
//
void L3::MeshEnetRUDPProcessor::_onEnetTimerTick() {
    auto rucbs = _copyRUCBs();
    for (auto it : rucbs) {
        std::shared_ptr<L3::MeshEnetRUCB> rucb = it.second;
        Anyfi::Core::getInstance()->enqueueTask([this, rucb] {
            _doEnetService(rucb);
        });
    }
}

void L3::MeshEnetRUDPProcessor::enetThreadProc() {
    std::stringstream ss;
    ss << "enetThreadProc";
    pthread_setname_np(pthread_self(), ss.str().c_str());

    while(!shouldShutdown) {

        auto rucbs = _copyRUCBs();
        for (auto it : rucbs) {
            std::shared_ptr<L3::MeshEnetRUCB> rucb = it.second;
            Anyfi::Core::getInstance()->enqueueTask([this, rucb] {
                _doEnetService(rucb);
            });
        }
        long delay_ms = rucbs.empty() ? 500 : 33;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
}

void L3::MeshEnetRUDPProcessor::_doEnetService(std::shared_ptr<MeshEnetRUCB> rucb) {
    if (rucb == nullptr)
        return;

    ENetEvent event{};

    uint8_t pendingOps = 0;
    NetworkBufferPointer buffer;

    {
        Anyfi::Config::lock_type guard(rucb->_mutex);

        if (rucb->state() == MeshEnetRUCB::State::CLOSED)
            return;

        auto host = rucb->host();
        if (host == nullptr)
            return;

        if (enet_host_service(host, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    pendingOps = _onEnetConnected(rucb, event);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    pendingOps = _onEnetDisconnected(rucb, event);
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    auto result = _onEnetReceived(rucb, event);
                    pendingOps = result.first;
                    buffer = result.second;
                    break;
                }

                case ENET_EVENT_TYPE_NONE:
                    return;
            }
        }
    }

    /**
     * Process pending operations outside the rucb lock.
     */
    if ((pendingOps & PENDING_OPS_NOTIFY_CONNECT_SUCCESS) == PENDING_OPS_NOTIFY_CONNECT_SUCCESS) {
        _notifyConnectCallback(rucb->key().srcAddress().port(), rucb);
    }
    if ((pendingOps & PENDING_OPS_NOTIFY_ACCEPTED) == PENDING_OPS_NOTIFY_ACCEPTED) {
        _L3->onSessionConnected(rucb);
    }
    if ((pendingOps & PENDING_OPS_NOTIFY_READ) == PENDING_OPS_NOTIFY_READ) {
        if (buffer != nullptr) {
            _L3->onReadFromPacketProcessor(rucb, buffer);
            enet_packet_destroy(event.packet);
        }
    }
    if ((pendingOps & PENDING_OPS_NOTIFY_CLOSED) == PENDING_OPS_NOTIFY_CLOSED) {
        _L3->onSessionClosed(rucb, true);
    }

    /**
     * Handle CLOSE_WAIT state
     */
    {
        Anyfi::Config::lock_type guard(rucb->_mutex);

        if (rucb->state() == MeshEnetRUCB::State::CLOSE_WAIT) {
            bool sentAll = true;
            auto *closedHost = rucb->host();
            for (int i = 0; i < closedHost->peerCount; ++i) {
                auto *peer = &closedHost->peers[i];
                if (!enet_list_empty(&peer->outgoingReliableCommands) ||
                    !enet_list_empty(&peer->sentReliableCommands)) {
                    sentAll = false;
                    break;
                }
            }

            if (sentAll) {
                _closeEnetPeer(rucb);
            }
        }
    }
}

uint8_t L3::MeshEnetRUDPProcessor::_onEnetConnected(std::shared_ptr<L3::MeshEnetRUCB> rucb,
                                                    ENetEvent &event) {
    switch (rucb->state()) {
        case MeshEnetRUCB::State::CONNECTING: {
            rucb->state(MeshEnetRUCB::State::CONNECTED);

            _writeAllPendingBuffers(rucb);

            return PendingOperation::PENDING_OPS_NOTIFY_CONNECT_SUCCESS;
        }
        case MeshEnetRUCB::State::ACCEPT_WAIT: {
            rucb->state(MeshEnetRUCB::State::CONNECTED);
            rucb->peer(event.peer);

            return PendingOperation::PENDING_OPS_NOTIFY_ACCEPTED;
        }
        default:
            Anyfi::Log::error(__func__, "Wrong state");
            throw std::runtime_error("Wrong state");
    }
}

uint8_t L3::MeshEnetRUDPProcessor::_onEnetDisconnected(std::shared_ptr<L3::MeshEnetRUCB> rucb,
                                                       ENetEvent &event) {
    uint8_t pendingOps = 0;

    switch (rucb->state()) {
        case MeshEnetRUCB::State::CONNECTING: {
            // Notify
            pendingOps = PENDING_OPS_NOTIFY_CONNECT_FAIL;
            break;
        }
        case MeshEnetRUCB::State::CONNECTED: {
            pendingOps = PENDING_OPS_NOTIFY_CLOSED;
            break;
        }
        case MeshEnetRUCB::State::ACCEPT_WAIT:
        case MeshEnetRUCB::State::CLOSE_WAIT:
        case MeshEnetRUCB::State::ENET_CLOSING:
        case MeshEnetRUCB::State::CLOSED:
            break;
    }

    rucb->state(MeshEnetRUCB::State::CLOSED);
    _removeRUCB(rucb->key());

    auto host = rucb->host();
    if (host != nullptr)
        enet_host_destroy(host);

    return pendingOps;
}

std::pair<uint8_t, NetworkBufferPointer>
L3::MeshEnetRUDPProcessor::_onEnetReceived(std::shared_ptr<L3::MeshEnetRUCB> rucb, ENetEvent &event) {
    if (rucb->state() != MeshEnetRUCB::State::CONNECTED) {
        Anyfi::Log::error(__func__, "State is not connected");

        enet_packet_destroy(event.packet);
        return std::pair<uint8_t, NetworkBufferPointer>(NULL, nullptr);
    }

    auto buffer = NetworkBufferPool::acquire();
    if(event.packet->dataLength + 64 > NETWORK_BUFFER_SIZE) {
        Anyfi::Log::error(__func__, "event.packet->dataLength + 64 > NETWORK_BUFFER_SIZE");
        assert(event.packet->dataLength + 64 <= NETWORK_BUFFER_SIZE);
    }
    buffer->putBytes(event.packet->data, event.packet->dataLength);

    if (buffer == nullptr) {
        Anyfi::Log::error(__func__, "receive buffer is nullptr");
        return std::pair<uint8_t, NetworkBufferPointer>(NULL, nullptr);
    }

    return std::pair<uint8_t, NetworkBufferPointer>(PendingOperation::PENDING_OPS_NOTIFY_READ,
                                                    buffer);
}
//
// MeshEnetRUDPProcessor protected methods
// ========================================================================================
//


//
// ========================================================================================
// MeshEnetRUDPProcessor private methods
//
void L3::MeshEnetRUDPProcessor::_addRUCB(const std::shared_ptr<MeshEnetRUCB> &rucb, ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_rucbMapMutex);
        _rucbMap[key] = rucb;
    }

    _L3->onControlBlockCreated(rucb);
}

void L3::MeshEnetRUDPProcessor::_removeRUCB(ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_rucbMapMutex);
        _rucbMap.erase(key);
    }

    _L3->releasePort(key.srcAddress().port());
    _L3->onControlBlockDestroyed(key);
}

std::shared_ptr<L3::MeshEnetRUCB> L3::MeshEnetRUDPProcessor::_findRUCB(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_rucbMapMutex);

    for (auto &item : _rucbMap) {
        if (item.second->key() == key)
            return item.second;
    }

    return nullptr;
}

std::shared_ptr<L3::MeshEnetRUCB> L3::MeshEnetRUDPProcessor::_findRUCB(ENetSocket sock) {
    Anyfi::Config::lock_type guard(_rucbMapMutex);

    for (auto &item : _rucbMap) {
        auto host = item.second->host();
        if (host == nullptr)
            continue;

        if (host->socket == sock)
            return item.second;
    }

    return nullptr;
}

std::unordered_map<ControlBlockKey, std::shared_ptr<L3::MeshEnetRUCB>, ControlBlockKey::Hasher> L3::MeshEnetRUDPProcessor::_copyRUCBs() {
    Anyfi::Config::lock_type guard(_rucbMapMutex);

    return _rucbMap;
}

void L3::MeshEnetRUDPProcessor::_keepConnectCallback(uint16_t localPort, L3::MeshEnetRUDPProcessor::ConnectCallback callback) {
    Anyfi::Config::lock_type guard(_connectCallbackMutex);

    _connectCallbacks[localPort] = callback;
}

void L3::MeshEnetRUDPProcessor::_notifyConnectCallback(uint16_t localPort, std::shared_ptr<ControlBlock> resultCB) {
    ConnectCallback callback;

    {
        Anyfi::Config::lock_type guard(_connectCallbackMutex);

        callback = _connectCallbacks[localPort];
    }

    if (callback == nullptr) {
        Anyfi::Log::error(__func__, "connect callback is nullptr");
        return;
    }

    callback(resultCB);
}

void L3::MeshEnetRUDPProcessor::_writeAllPendingBuffers(std::shared_ptr<L3::MeshEnetRUCB> rucb) {
    if (rucb->state() != MeshEnetRUCB::State::CONNECTED) {
        Anyfi::Log::error(__func__, "rucb state should be CONNECTED");
        return;
    }

    auto it = rucb->_writeBuffers.begin();
    while (it != rucb->_writeBuffers.end()) {
        _writeToEnet(rucb, *it);

        it = rucb->_writeBuffers.erase(it);
    }
}

void
L3::MeshEnetRUDPProcessor::_writeToEnet(const std::shared_ptr<L3::MeshEnetRUCB> &rucb, NetworkBufferPointer buffer) {
    if (rucb == nullptr) {
        Anyfi::Log::error(__func__, "rucb is nullptr");
        return;
    }

    auto peer = rucb->peer();
    if (peer == nullptr) {
        Anyfi::Log::error(__func__, "peer is nullptr");
        return;
    }

    auto *packet = enet_packet_create(buffer->buffer(),
                                      buffer->getWritePos() - buffer->getReadPos(),
                                      ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

void L3::MeshEnetRUDPProcessor::_closeEnetPeer(std::shared_ptr<L3::MeshEnetRUCB> rucb) {
    auto peer = rucb->peer();
    if (peer == nullptr) {
        Anyfi::Log::error(__func__, "peer is nullptr");
        return;
    }

    enet_peer_disconnect(peer, 0);
    rucb->clearWriteBuffer();
    rucb->state(MeshEnetRUCB::State::ENET_CLOSING);
}
//
// MeshEnetRUDPProcessor private methods
// ========================================================================================
//


//
// ========================================================================================
// Extern Enet functions
//
extern "C" int enet_socket_bind(ENetSocket socket, const ENetAddress *address) {
    return 0;
}

extern "C" int enet_socket_get_address(ENetSocket socket, ENetAddress *address) {
    return -1;
}

extern "C" int enet_socket_listen(ENetSocket socket, int backlog) {
    throw std::runtime_error("not implemented yet");
}

extern "C" ENetSocket enet_socket_create(ENetSocketType type) {
    return (ENetSocket) 1;
}

extern "C" int enet_socket_set_option(ENetSocket socket, ENetSocketOption option, int value) {
    return 0;
}

extern "C"
int enet_socket_get_option(ENetSocket socket, ENetSocketOption option, int *value) {
    return 0;
}

extern "C"
int enet_socket_connect(ENetSocket socket, const ENetAddress *address) {
    throw std::runtime_error("not implemented yet");
}

extern "C"
ENetSocket enet_socket_accept(ENetSocket socket, ENetAddress *address) {
    throw std::runtime_error("not implemented yet");
}

extern "C"
int enet_socket_shutdown(ENetSocket socket, ENetSocketShutdown how) {
    throw std::runtime_error("not implemented yet");
}

extern "C"
void enet_socket_destroy(ENetSocket socket) {
    return;
}

/**
 * Use this ENET_API
 */
extern "C"
int enet_socket_send(ENetSocket socket, const ENetAddress *address, const ENetBuffer *buffers,
                     size_t bufferCount) {
    auto processor = L3::MeshEnetRUDPProcessor::getInstance();
    return processor->enetAPISend(socket, address, buffers, bufferCount);
}

/**
 * Use this ENET_API
 */
extern "C"
int enet_socket_receive(ENetSocket socket, ENetAddress *address, ENetBuffer *buffers,
                        size_t bufferCount) {
    auto processor = L3::MeshEnetRUDPProcessor::getInstance();
    return processor->enetAPIRecv(socket, address, buffers, bufferCount);
}

extern "C"
int enet_socketset_select(ENetSocket maxSocket, ENetSocketSet *readSet, ENetSocketSet *writeSet,
                          enet_uint32 timeout) {
    throw std::runtime_error("Not yet implemented");
}

extern "C"
int enet_socket_wait(ENetSocket socket, enet_uint32 *condition, enet_uint32 timeout) {
    return 0;
}

extern "C" void enet_log(ENetHost *host, const char *text) {
}

extern "C" void enet_log_dump(ENetHost *host, const char *tag, const char *buf, int len) {
}
//
// Extern Enet functions
// ========================================================================================
//