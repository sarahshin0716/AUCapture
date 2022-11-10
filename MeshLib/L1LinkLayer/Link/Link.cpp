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

#include "Link.h"

#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "L1LinkLayer/L1LinkLayer.h"
#include "Log/Frontend/Log.h"


//
// ========================================================================================
// Link class code
//
Link::Link(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel)
: _selector(std::move(selector)), _channel(std::move(channel)) {
    writtenBytes = readBytes = prevWrittenBytes = prevReadBytes = 0;
}

Link::Link(std::shared_ptr<Selector> selector, std::shared_ptr<BaseChannel> channel, Anyfi::Address link_addr)
: _selector(std::move(selector)), _channel(std::move(channel)), _linkAddr(std::move(link_addr)) {
    writtenBytes = readBytes = prevWrittenBytes = prevReadBytes = 0;
}

Link::~Link() {
}

const char *Link::stateStr() const {
    switch (_state) {
        case LinkState::READY:
            return "READY";
        case LinkState::SOCK_CONNECTING:
            return "SOCK_CONNECTING";
        case LinkState::LINK_HANDSHAKING:
            return "LINK_HANDSHAKING";
        case LinkState::CONNECTED:
            return "CONNECTED";
        case LinkState::DISCONNECTED:
            return "DISCONNECTED";
        case LinkState::CLOSED:
            return "CLOSED";
        default:
            return "Check Link#stateStr()";
    }
}

void Link::connect(Anyfi::Address address, std::function<void(bool result)> callback) {
    connect(address, "", callback);
}

void Link::connect(Anyfi::Address address, const std::string &ifname, std::function<void(bool result)> callback) {
    if (address.isEmpty())
        throw std::invalid_argument("Empty addr");
    if (address.isMeshProtocol())
        throw std::invalid_argument("Connection protocol should not be mesh protocol");

    Anyfi::Config::lock_type guard(_operationMutex);

    _linkAddr = address;
    _sockConnectStart = std::chrono::system_clock::now();   // Set socket-connect start time
    _connectCallback = callback;

    switch (doSockConnect(std::move(address), ifname)) {
        case Anyfi::SocketConnectResult::CONNECTED:
            onSockConnectFinished(true);
            break;
        case Anyfi::SocketConnectResult::CONNECT_FAIL:
            onSockConnectFinished(false);
            break;
        case Anyfi::SocketConnectResult::CONNECTING:
            state(LinkState::SOCK_CONNECTING);
            _selectionKey = registerOperation(_selector, SelectionKeyOp::OP_CONNECT);
            break;
    }
}

bool Link::open(Anyfi::Address address, const std::string& in) {
    _linkAddr = address;

    bool result = doOpen(std::move(address), in);
    if (result) {
        int interestOps = 0;

        if (address.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            // TOOD : new state required
            interestOps = SelectionKeyOp::OP_ACCEPT;
        } else if (address.connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
            state(LinkState::CONNECTED);
            interestOps = SelectionKeyOp::OP_READ;
        }

        _selectionKey = registerOperation(_selector, interestOps);
    }

    return result;
}

void Link::close() {
    closeWithoutNotify();

    if (_channel)
        _channel->onClose(shared_from_this());
}

void Link::closeWithoutNotify() {
    Anyfi::Config::lock_type guard(_operationMutex);

    if (_selectionKey != nullptr)
        _selectionKey->cancel();

    if (state() != LinkState::CLOSED) {
        clearWriteQueue();

        doClose();

        if (_state == LinkState::SOCK_CONNECTING || _state == LinkState::LINK_HANDSHAKING) {
            notifyConnectResult(false);
        }
        state(LinkState::CLOSED);
    }
}

void Link::write(NetworkBufferPointer buffer) {
    Anyfi::Config::lock_type guard(_operationMutex);

    if (_state == LinkState::CLOSED)
        return;

    addToWriteQueue(std::move(buffer));

    registerOperation(_selector, SelectionKeyOp::OP_WRITE);
}

std::shared_ptr<SelectionKey> Link::registerOperation(std::shared_ptr<Selector> selector, int ops) {
    if ((ops & ~validOps()) != 0)
        throw std::invalid_argument("Invalid Ops");

    if (state() == LinkState::CLOSED)
        return nullptr;

    Anyfi::Config::lock_type guardWakeup(selector->_wakeupLock);
    if (_selectionKey == nullptr) {
        // New registration
        selector->wakeup();
        _selectionKey = selector->registerOps(shared_from_this(), ops);
    } else {
        // Add interest ops
        _selectionKey->registerInterestOpsNoCheck(ops);
    }
    selector->wakeup();

    return _selectionKey;
}

void Link::deregisterOperation(int ops) {
    if ((ops & ~validOps()) != 0)
        throw std::invalid_argument("Invalid Ops");

    if (state() == LinkState::CLOSED)
        return;

    if (_selectionKey != nullptr) {
        // Remove interest ops
        _selectionKey->deregisterInterestOpsNoCheck(ops);
    }
}

void Link::addToWriteQueue(NetworkBufferPointer buffer) {
    // Lock this method to make writeQueue thread-safe.
    Anyfi::Config::lock_type guard(_writeQueueMutex);
    _writeQueue.push_back(buffer);
}

void Link::clearWriteQueue() {
    // Lock this method to make writeQueue thread-safe.
    Anyfi::Config::lock_type guard(_writeQueueMutex);

    _writeQueue.clear();
}

bool Link::checkTimeout() {
    Anyfi::Config::lock_type guard(_operationMutex);

    if (_state == LinkState::SOCK_CONNECTING) {
        bool isTimeout = _checkSockTimeout();
        if (isTimeout) {
            clearWriteQueue();
            doClose();
            onSockConnectFinished(false);
        }
        return isTimeout;
    }
    if (_state == LinkState::LINK_HANDSHAKING) {
        bool isTimeout = checkHandshakeTimeout();
        if (isTimeout) {
            onLinkHandshakeFinished(false);
        }
        return isTimeout;
    }

    return false;
}

void Link::notifyConnectResult(bool result) {
    if (_connectCallback != nullptr) {
        _connectCallback(result);
        _connectCallback = nullptr;
    }
}

void Link::onSockConnectFinished(bool result) {
    deregisterOperation(SelectionKeyOp::OP_CONNECT);

    if (!result) {
        state(LinkState::DISCONNECTED);
        notifyConnectResult(false);
        return;
    }

    registerOperation(_selector, SelectionKeyOp::OP_READ);

    if (isHandshakeRequired()) {
        state(LinkState::LINK_HANDSHAKING);
        doHandshake();
    } else {
        state(LinkState::CONNECTED);
        notifyConnectResult(true);
    }
}

void Link::onLinkHandshakeFinished(bool result) {
    state(result ? LinkState::CONNECTED : LinkState::DISCONNECTED);
    notifyConnectResult(result);
}

void Link::protectFromVPN() {
    if (L1LinkLayer::getInstance() == nullptr)
        return;

    auto vpnChannel = L1LinkLayer::getInstance()->_vpnChannel;
    if (vpnChannel != nullptr && vpnChannel->state() == BaseChannel::State::CONNECTED) {
        vpnChannel->protect(_sock);
    }
}

bool Link::_checkSockTimeout() {
    if (_state != LinkState::SOCK_CONNECTING)
        return false;

    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _sockConnectStart).count() > SOCK_CONNECT_TIMEOUT_MILLI_SEC;
}
//
// Link class codes
// ========================================================================================
//


NetworkBufferPointer _readFromSock(int sock) {
    auto buffer = NetworkBufferPool::acquire();
    buffer->resize((uint32_t) buffer->capacity());
    auto readBytes = Anyfi::Socket::read(sock, buffer);

    if (readBytes <= 0) {
        return nullptr;
    }

    buffer->resize((uint32_t) readBytes);
    buffer->setWritePos((uint32_t) readBytes);
    return buffer;
}

NetworkBufferPointer _readFromSock(int sock, size_t limit) {
    auto buffer = NetworkBufferPool::acquire();
    buffer->resize((uint32_t) buffer->capacity());
    auto readBytes = Anyfi::Socket::read(sock, buffer, limit);

    if (readBytes <= 0) {
        return nullptr;
    }

    buffer->resize((uint32_t) readBytes);
    buffer->setWritePos((uint32_t) readBytes);
    return buffer;
}

//
const std::string Link::toString()
{
    std::string dumped = "link " + to_string(this)
                         + " state=" + to_string((int)state())
                         + " channel=" + to_string(channel().get())
                         + " writeQueueSize=" + to_string(_writeQueue.size())
                         + " readBytes=" + to_string(readBytes)
                         + " writtenBytes=" + to_string(writtenBytes)
    ;
    return dumped;
}
