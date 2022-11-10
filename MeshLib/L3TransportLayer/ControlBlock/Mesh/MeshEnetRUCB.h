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

#ifndef ANYFIMESH_CORE_MESHENETRUCB_H
#define ANYFIMESH_CORE_MESHENETRUCB_H

#include "../../Processor/Mesh/EnetRUDP/enet/enet.h"
#include "MeshControlBlock.h"


namespace L3 {

// Forward declaration : L3::MeshEnetRUDPProcessor
class MeshEnetRUDPProcessor;

/**
 * Mesh Enet RUDP control block.
 */
class MeshEnetRUCB : public MeshControlBlock {
public:
    enum class State : unsigned short {
        /**
         * Represents no connection state at all.
         */
        CLOSED                      = 0,

//
// ========================================================================================
// Connections not established.
//
        /**
         *
         */
        ACCEPT_WAIT                 = 1,

        /**
         *
         */
        CONNECTING                  = 2,
//
// Connections not established.
// ========================================================================================
//

        /**
         *
         */
        CONNECTED                   = 3,

        /**
         *
         */
        CLOSE_WAIT                  = 4,

        /**
         *
         */
        ENET_CLOSING                = 5,
    };

    explicit MeshEnetRUCB(ControlBlockKey &key);
    ~MeshEnetRUCB();

//
// ========================================================================================
// RUCB Getter/Setter
//
    State state() const { return _state; }
    void state(State state) { _state = state; }

    ENetHost *host() const { return _host; }
    void host(ENetHost *host) { _host = host; }

    ENetPeer *peer() const { return _peer; }
    void peer(ENetPeer *peer) { _peer = peer; }
//
// RUCB Getter/Setter
// ========================================================================================
//

    void putReadBuffer(NetworkBufferPointer buffer);
    NetworkBufferPointer popReadBuffer();

    void putWriteBuffer(NetworkBufferPointer buffer);
    void clearWriteBuffer();

private:
    friend class L3::MeshEnetRUDPProcessor;

    // Locks state and enet variables
    Anyfi::Config::mutex_type _mutex;

    State _state;

    // Enet variables
    ENetHost *_host = nullptr;
    ENetPeer *_peer = nullptr;

    // Read buffers
    Anyfi::Config::mutex_type _readBufMutex;
    std::list<NetworkBufferPointer> _readBuffers;

    // Write buffers
    Anyfi::Config::mutex_type _writeBufMutex;
    std::list<NetworkBufferPointer> _writeBuffers;
};

}   // namespace L3


#endif //ANYFIMESH_CORE_MESHENETRUCB_H
