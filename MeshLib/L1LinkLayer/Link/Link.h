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

#ifndef ANYFIMESH_CORE_LINK_H
#define ANYFIMESH_CORE_LINK_H

#include <mutex>
#include <memory>
#include <utility>

#include "../../Common/Network/Address.h"
#include "../../Common/Network/Sock/Socket.h"
#include "../../Common/Network/Buffer/NetworkBuffer.h"

#include "../../L1LinkLayer/Channel/Channel.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif


#define SOCK_CONNECT_TIMEOUT_MILLI_SEC 5000
#define LINK_READ_BUFFER_SIZE 65536


// Forward declarations : Selector.h
enum SelectionKeyOp : unsigned short;
class SelectionKey;
class Selector;

// Forward declarations : Channel.h
class BaseChannel;

// Forward declarations : this
class Link;


/**
 * A Link state.
 * A link can be in one of the following states.
 */
enum class LinkState {
    /**
     * A link that has not yet connected is in this state.
     */
    READY,

    /**
     * A link that is connecting socket is in this state.
     */
    SOCK_CONNECTING,

    /**
     * A link that is handshaking is in this state.
     */
    LINK_HANDSHAKING,

    /**
     * A link that is connected and possible to read or write is in this state.
     */
    CONNECTED,

    /**
     * A link that has disconnected is in this state.
     * Not yet closed in this state.
     */
    DISCONNECTED,

    /**
     * A link's socket has closed is in this state.
     * All resources in the link are released.
     */
    CLOSED,
};


/**
 * A link handshake result.
 * It is used as the return value of the function {@link #processHandshake}, and {@link #doHandshake}.
 */
enum class LinkHandshakeResult {
    /**
     * A link handshake is in progress.
     */
    HANDSHAKING = 0,

    /**
     * A link handshake has been successfully completed.
     * And link has been connected.
     */
    CONNECT_SUCCESS = 1,

    /**
     * A link handshake has been successfully completed.
     * And link has been accepted.
     */
    ACCEPT_SUCCESS = 2,

    /**
     * A link handshake failed.
     */
    HANDSHAKE_FAILED = -1,
};

NetworkBufferPointer _readFromSock(int sock);

NetworkBufferPointer _readFromSock(int sock, size_t limit);

/**
 * A nexus for network I/O operations.
 *
 * A link represents an open connection to a network socket.
 * Links are, in general, intended to be safe for multi-threaded access.
 *
 * This link that can be multiplexed via a {@link Selector}.
 * In order to be used with a selector, an instance of this class must first be registered via the
 * {@link Link#registerOperation} method. This method returns a new {@link SelectionKey} object
 * that represents the link's registration with the selector.
 *
 * This link is in non-blocking networkMode, every I/O operation public methods will never block and may transfer fewer bytes
 * than were requested or possibly no bytes at all.
 * Actual I/O operations should be performed via protected methods such as doSockConnect, doRead, and doWrite in the {@link IOEngine}.
 */
class Link : public std::enable_shared_from_this<Link> {
public:
    /**
     * Connect this link's socket.
     *
     * @param address The remote address to which this link is to be connected.
     * @param callback Connect result callback method. result is true, if connect is success, false otherwise.
     */
    void connect(Anyfi::Address address, std::function<void(bool result)> callback);
    /**
     * connect method와 동일하며, 연결에 활용할 네트워크 인터페이스 지정 파라미터 추가. 이는 IPv6 connect에 활용
     * @param address 연결할 주소
     * @param ifname 연결에 사용될 네트워크 인터페이스 이름
     * @param callback 연결 결과 콜백 메소드
     */
    void connect(Anyfi::Address address, const std::string &ifname, std::function<void(bool result)> callback);
    /**
     * Open this link's socket as a server.
     *
     * @param address The local address to open.
     * @return true if open is success, false otherwise.
     */
    bool open(Anyfi::Address address, const std::string& in = "");
    /**
     * Close this link's socket connection.
     */
    void close();
    void closeWithoutNotify();
    /**
     * Writes a sequence of bytes to this link from the given {@link NetworkBuffer}
     *
     * @param buffer The buffer from which bytes are to be retrieved.
     */
    virtual void write(NetworkBufferPointer buffer);

    std::shared_ptr<SelectionKey> selectionKey() const { return _selectionKey; }
    std::shared_ptr<BaseChannel> channel() { return _channel; }
    int sock() const { return _sock; }
    Anyfi::Address addr() const { return _linkAddr; }
    LinkState state() const { return _state; }
    const char *stateStr() const;
    std::shared_ptr<Selector> selector() const { return _selector; }

    void channel(const std::shared_ptr<BaseChannel> &channel) { _channel = channel; }

    inline bool operator==(const Link &other) const { return this->addr() == other.addr() && this->sock() == other.sock(); }
    inline bool operator!=(const Link &other) const { return this->addr() != other.addr() || this->sock() != other.sock(); }

protected:
    friend class IOEngine;
    friend class Selector;
    friend class SelectionKey;
    friend class L1LinkLayer;
    friend class WifiP2PChannel;

    Link(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel);
    Link(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel, Anyfi::Address link_addr);
    virtual ~Link();

    /**
     * Registers this link with the given selector, returning a selection key.
     *
     * @param selector The selector with which this link to be registered.
     * @param ops The interest set for the resulting key
     * @return A key representing the registration of this link with the given selector.
     */
    virtual std::shared_ptr<SelectionKey> registerOperation(std::shared_ptr<Selector> selector, int ops);
    /**
     * Deregisters the given operations from the given selector.
     *
     * @param ops The operation set to dereigster.
     */
    virtual void deregisterOperation(int ops);

    /**
     * Check this link is timed out.
     * This method impl checks socket-connect, and link-handshake timeout.
     * If link-handshake need to retransmit a handshake packet, do so.
     *
     * @return true if timed out and needs to be cleaned up, false otherwise.
     */
    bool checkTimeout();

    /**
     * Returns an operation set identifying this link's supported operations.
     * This bits that are set in this integer value denote exactly the operations that are valid for this link.
     * This method always returns same value for a given concrete link class.
     *
     * @return The valid-operation set.
     */
    virtual int validOps() const = 0;

    /**
     * Performs actual socket connect operation.
     * This method can only be invoked at {@link IOEngine}.
     *
     * As you know, this link's socket is in non-blocking networkMode, so invocation of this method start
     * It the connection is established immediately as can happen with a local connection, then this method return true.
     * Otherwise this method returns false and the connection operation must be completed by invoking the {@link #finishConnect} method.
     *
     * @param address The remote address to which this link is to be connected.
     */
    virtual Anyfi::SocketConnectResult doSockConnect(Anyfi::Address address, const std::string &ifname) = 0;
    /**
     * Finishes the process of connecting a socket.
     * This method can only be invoked at {@link IOEngine}.
     *
     * If link is readable or writable after perform doSockConnect,
     * check connect is success and finish connect operation by invoking this method.
     *
     * @return true if connect is success, false otherwise.
     */
    virtual bool finishSockConnect() = 0;

    /**
     * Returns whether this link requires a handshake.
     *
     * @return true if handshake is required, false otherwise.
     */
    virtual bool isHandshakeRequired() = 0;
    /**
     * Do link handshake.
     *
     * @return handshake result.
     */
    virtual LinkHandshakeResult doHandshake() = 0;
    /**
     * Process a link handshake packet.
     * If IOEngine reads packets while in LINK_HANDSHAKE state, should process handshake packet by invoking this method.
     *
     * @param buffer A buffer of link handshake packet.
     * @return handshake result.
     */
    virtual LinkHandshakeResult processHandshake(NetworkBufferPointer buffer) = 0;
    /**
     * Checks the link-handshake is timed out.
     */
    virtual bool checkHandshakeTimeout() = 0;

    /**
     * Performs actual socket open operation.
     * This method can only be invoked at {@link IOEngine}.
     *
     * @param address address The local address to open.
     * @return true if open is success, false otherwise.
     */
    virtual bool doOpen(Anyfi::Address address, const std::string& in = "") = 0;
    /**
     * Performs actual link accept operation.
     * This method can only be invoked at {@link IOEngine}.
     *
     * @return Link New accepted link object.
     */
    virtual std::shared_ptr<Link> doAccept() = 0;

    /**
     * Performs actual link close operation.
     * This method can only be invoked at {@link IOEngine}.
     */
    virtual void doClose() = 0;

    /**
     * Performs actual socket close operation.
     * This method can only be invoked at {@link IOEngine}.
     *
     * @return A NetworkBuffer which contains read bytes.
     */
    virtual NetworkBufferPointer doRead() = 0;
    /**
     * Performs actual socket write operation.
     * This method can only be invoked at {@link IOEngine}.
     *
     * @param buffer The buffer from which bytes are to be retrieved.
     */
    virtual int doWrite(NetworkBufferPointer buffer) = 0;

    /**
     * Adds a given buffer to link's writeQueue.
     *
     * @param buffer The NetworkBuffer to put in link's writeQueue.
     */
    void addToWriteQueue(NetworkBufferPointer buffer);

    /**
     * Clear link's writeQueue.
     *
     */
    void clearWriteQueue();

    /**
     * Notify a connect-operation result by invoking connect callback function.
     * It makes {@link #_connectCallback} nullptr, after notify result.
     */
    void notifyConnectResult(bool result);

    /**
     * Callback method which called when socket-connect operation has finished.
     */
    void onSockConnectFinished(bool result);
    /**
     * Callback method which called when link-handshake operation has finished.
     */
    void onLinkHandshakeFinished(bool result);

    /**
     * Protects a socket from the vpn connection.
     */
    void protectFromVPN();

    void state(LinkState state) { _state = state; }

private:
    LinkState _state = LinkState::READY;
    std::shared_ptr<BaseChannel> _channel;

    // Socket connect start time value, it used to check socket-connect timeout.
    bool _checkSockTimeout();

    // Connect result callback
    std::function<void(bool result)> _connectCallback;

    std::list<NetworkBufferPointer> _writeQueue;
    Anyfi::Config::mutex_type _writeQueueMutex;

protected:
    const std::shared_ptr<Selector> _selector;
    Anyfi::Address _linkAddr;
    int _sock = -1;
    size_t _readLimit = LINK_READ_BUFFER_SIZE;

    // Locks a operation which modify internal data structures.
    Anyfi::Config::mutex_type _operationMutex;

    // Last link handshake operation time value, it used to check link-handshake timeout.
    std::chrono::time_point<std::chrono::system_clock> _lastHandshake;

    std::shared_ptr<SelectionKey> _selectionKey;

public:
    int prevReadBytes;
    int prevWrittenBytes;
    int readBytes;
    int writtenBytes;
    const std::string toString();

    std::chrono::time_point<std::chrono::system_clock> _sockConnectStart;
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(Selector, Cancel);
    FRIEND_TEST(Link, Create);
    FRIEND_TEST(Link, RegisterOperation);
    FRIEND_TEST(Link, DeregisterOperation);
    FRIEND_TEST(L1LinkLayer, AcceptWifiP2PLink);
    FRIEND_TEST(L1LinkLayer, connect2L1LinkLayer);
    FRIEND_TEST(ProxyChannel, Connect);
#endif
};


//class HandshakeLink : public Link {
//public:
//    explicit HandshakeLink(std::shared_ptr<Link> parent)
//            : Link(parent->selector()), _parent(parent) {}
//    HandshakeLink(const int sock, std::shared_ptr<Selector> selector) : Link(std::move(selector)) {
//        _sock = sock;
//    }
//
//    std::shared_ptr<Link> parent() const { return _parent; }
//    void parent(const std::shared_ptr<Link> &parent) { _parent = parent; }
//
//protected:
//    std::shared_ptr<Link> _parent;
//};


#endif //ANYFIMESH_CORE_LINK_H
