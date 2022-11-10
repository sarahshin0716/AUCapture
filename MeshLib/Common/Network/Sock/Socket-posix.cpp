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

// Socket-posix.cpp should only be compiled on unix
#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
    #elif TARGET_OS_IPHONE
    #elif TARGET_OS_MAC
        #define POSIX
    #endif
#elif __linux__ || __unix__ || defined(_POSIX_VERSION)
    #define POSIX
#endif

#ifdef POSIX


#include "Socket.h"

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <Log/Frontend/Log.h>
#include <net/if.h>

#include <android/multinetwork.h>
#include <linux/tcp.h>

#include "Core.h"
#include "Log/Backend/LogManager.h"

//
// ========================================================================================
// Posix socket operation impl declaration
//
Anyfi::SocketConnectResult _connectImpl(int sock, Anyfi::Address address, const std::string &in);

bool _openImpl(int sock, const Anyfi::Address &address, const std::string& in);

int _acceptImpl(int sock);

int _readImpl(int sock, NetworkBufferPointer buffer);

int _readImpl(int sock, NetworkBufferPointer buffer, size_t limit);

int _writeImpl(int sock, NetworkBufferPointer buffer);

int _sendImpl(int sock, NetworkBufferPointer buffer);

int _sendToImpl(int sock, NetworkBufferPointer buffer, Anyfi::Address dstAddr, uint32_t scopeId);

void _closeImpl(int sock);
//
// Posix socket operation impl declaration
// ========================================================================================
//


//
// ========================================================================================
// Socket class
//
int Anyfi::Socket::create(const Anyfi::Address &addr, bool blocking) {
    int domain = 0;
    switch (addr.type()) {
        case Anyfi::AddrType::IPv4:
            domain = AF_INET;
            break;
        case Anyfi::AddrType::IPv6:
            domain = AF_INET6;
            break;
        default:
            throw std::invalid_argument("Invalid Address Type.");
    }

    int type = 0;
    int protocol = 0;
    int sndBufSize = -1;
    socklen_t optlen = sizeof(sndBufSize);
    switch (addr.connectionProtocol()) {
        case Anyfi::ConnectionProtocol::TCP:
            type = SOCK_STREAM;
            protocol = IPPROTO_TCP;
            sndBufSize = 1000000;
            break;
        case Anyfi::ConnectionProtocol::UDP:
        case Anyfi::ConnectionProtocol::MeshUDP:
        case Anyfi::ConnectionProtocol::MeshRUDP:
            type = SOCK_DGRAM;
            protocol = IPPROTO_UDP;
            sndBufSize = 2000000;
            break;
        default:
            throw std::invalid_argument("Invalid Connection Protocol.");
    }

    int sock = socket(domain, type, protocol);
    if (sock >= 1024) {
        Anyfi::LogManager::getInstance()->record()->socketCreateFailed(sock);
        close(sock);
        return -1;
    }

    int opt = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    // Bind network
    net_handle_t netHandle = Anyfi::Core::getInstance()->getNetworkHandle(addr.networkType());
    if(netHandle > 0) {
        int ret = android_setsocknetwork(netHandle, sock);
    }

    if (!blocking) {
        // Configure non-blocking
        if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
            close(sock);
            return -1;
        }
    }

    return sock;
}

Anyfi::SocketConnectResult Anyfi::Socket::connect(int sock, const Anyfi::Address &address, const std::string &in) {
    return _connectImpl(sock, address, in);
}

Anyfi::SocketConnectResult Anyfi::Socket::connectResult(int sock) {
    int err = 0;
    socklen_t errLen = sizeof(err);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *) &err, &errLen) < 0) {
        // Get socket option failed
        close(sock);
        return Anyfi::SocketConnectResult::CONNECT_FAIL;
    }

    if (err != 0) {
        if (err == ECONNREFUSED) {
            // Connect refused.
        } else if (err == ETIMEDOUT) {
            // Socket-connect timeout a kernel level.
        }
        close(sock);
        return Anyfi::SocketConnectResult::CONNECT_FAIL;
    }

    return Anyfi::SocketConnectResult::CONNECTED;
}

bool Anyfi::Socket::open(int sock, const Anyfi::Address &addr, const std::string& in) {
    return _openImpl(sock, addr, in);
}

int Anyfi::Socket::accept(int sock) {
    return _acceptImpl(sock);
}

int Anyfi::Socket::read(int sock, NetworkBufferPointer buffer) {
    return _readImpl(sock, std::move(buffer));
}

int Anyfi::Socket::read(int sock, NetworkBufferPointer buffer, size_t limit) {
    return _readImpl(sock, std::move(buffer), limit);
}

int Anyfi::Socket::write(int sock, NetworkBufferPointer buffer) {
    return _writeImpl(sock, std::move(buffer));
}

int Anyfi::Socket::send(int sock, NetworkBufferPointer buffer) {
    return _sendImpl(sock, std::move(buffer));
}
int Anyfi::Socket::sendTo(int sock, NetworkBufferPointer buffer, Anyfi::Address dstAddr, int scopeId) {
    return _sendToImpl(sock, std::move(buffer), dstAddr, scopeId);
}

void Anyfi::Socket::close(int sock) {
    _closeImpl(sock);
}

Anyfi::Address Anyfi::Socket::getSockAddress(int sock) {
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    auto addr = Anyfi::Address();
    char buf[46];

    if (getsockname(sock, (struct sockaddr *)&sin, &len) < 0)
        return addr;

    inet_ntop(sin.sin_family, &sin.sin_addr.s_addr, buf, sizeof(buf));
    addr.port(ntohs(sin.sin_port));

    if (sin.sin_family == AF_INET) {
        addr.type(Anyfi::AddrType::IPv4);
        addr.setIPv4Addr(ntohl(sin.sin_addr.s_addr));
    }else if (sin.sin_family == AF_INET6) {
        addr.type(Anyfi::AddrType::IPv6);
    }
    return addr;
}
//
// Socket class
// ========================================================================================
//


//
// ========================================================================================
// Posix socket operation impl definition
//
Anyfi::SocketConnectResult _connectImpl(int sock, Anyfi::Address address, const std::string &in) {
    int result = 0;
    if (address.type() == Anyfi::AddrType::IPv4) { // IPv4 socket connect
        struct sockaddr_in addr{};
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port());
        addr.sin_addr.s_addr = inet_addr(address.getIPv4Addr().c_str());

        // bind
        if (!in.empty()) {
            Anyfi::Address bindAddr = Anyfi::Address::getIPv4AddrFromInterface(in);

            struct sockaddr_in sdaddr{};
            memset(&sdaddr, 0, sizeof(sdaddr));
            sdaddr.sin_family = AF_INET;
            sdaddr.sin_port = 0;
            sdaddr.sin_addr.s_addr = inet_addr(bindAddr.getIPv4Addr().c_str());

            if (bind(sock, (struct sockaddr *) &sdaddr, sizeof(sdaddr)) < 0) {
                close(sock);
                return Anyfi::SocketConnectResult::CONNECT_FAIL;
            }
        }
        result = connect(sock, (struct sockaddr *) &addr, sizeof(addr));
    } else if (address.type() == Anyfi::AddrType::IPv6) { // IPv6 socket connect
        struct sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(address.port());
        address.getIPv6AddrAsRawBytes((uint8_t *)&addr.sin6_addr);
        if (!in.empty())
            addr.sin6_scope_id = if_nametoindex(in.c_str());
        result = connect(sock, (struct sockaddr*) &addr, sizeof(addr));
    } else {
        result = -1;
    }

    if (result == 0) {
        // The connection is established immediately when connect to local address.
        return Anyfi::SocketConnectResult::CONNECTED;
    }
    if (errno != EINPROGRESS) {
        // Connect failed
        close(sock);
        return Anyfi::SocketConnectResult::CONNECT_FAIL;
    }

    return Anyfi::SocketConnectResult::CONNECTING;
}

bool _openImpl(int sock, const Anyfi::Address &address, const std::string& in = "") {
    if (address.type() == Anyfi::AddrType::IPv4) {
        struct sockaddr_in addr{};
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port());
        addr.sin_addr.s_addr = inet_addr(address.addr().c_str());

        if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            Anyfi::Log::error(__func__, std::string("Socket bind failed : ") + strerror(errno));
            return false;
        }

        if (address.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            if (listen(sock, 10) < 0) {
                Anyfi::Log::error(__func__, std::string("Socket listen failed : ") + strerror(errno));
                return false;
            }
        }
        return true;
    }else if (address.type() == Anyfi::AddrType::IPv6) {
        struct sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(address.port());
        addr.sin6_addr = IN6ADDR_ANY_INIT;
        if(!address.addr().empty())
            inet_pton(AF_INET6, address.addr().c_str(), (void *) &addr.sin6_addr);
        if (!in.empty())
            addr.sin6_scope_id = if_nametoindex(in.c_str());
        if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            Anyfi::Log::error(__func__, std::string("Socket bind failed : ") + strerror(errno));
            return false;
        }

        if (address.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            if (listen(sock, 10) < 0) {
                Anyfi::Log::error(__func__, std::string("Socket listen failed : ") + strerror(errno));
                return false;
            }
        }else if (address.connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
            return true;
        }
    }

    return false;
}


int _acceptImpl(int sock) {
    return accept(sock, nullptr, nullptr);
}


int _readImpl(int sock, NetworkBufferPointer buffer) {
    return _readImpl(sock, buffer, buffer->capacity());
}

int _readImpl(int sock, NetworkBufferPointer buffer, size_t limit) {
    ssize_t readBytes = read(sock, buffer->buffer(), (buffer->capacity() < limit ? buffer->capacity() : limit));

    if (readBytes > 0) {
        buffer->setWritePos((uint32_t) readBytes);
    }

    return readBytes;
}

int _writeImpl(int sock, NetworkBufferPointer buffer) {
    int writeBytes;
    if (buffer->isBackwardMode())
        writeBytes = write(sock, buffer->buffer(), buffer->getWritePos() - buffer->getReadPos());
    else
        writeBytes = write(sock, &buffer->buffer()[buffer->getReadPos()],
                           buffer->getWritePos() - buffer->getReadPos());

    if (writeBytes < 0) {
        return 0;
    }

    buffer->setReadPos(buffer->getReadPos() + writeBytes);
    return writeBytes;
}
int _sendImpl(int sock, NetworkBufferPointer buffer) {
    ssize_t writeBytes;
    if (buffer->isBackwardMode())
        writeBytes = send(sock, buffer->buffer(), buffer->getWritePos() - buffer->getReadPos(),0);
    else
        writeBytes = send(sock, &buffer->buffer()[buffer->getReadPos()],
                           buffer->getWritePos() - buffer->getReadPos(),0);

    if (writeBytes < 0) {
        return -1;
    }

    buffer->setReadPos(buffer->getReadPos() + (uint32_t)writeBytes);
    return (int)writeBytes;
}
int _sendToImpl(int sock, NetworkBufferPointer buffer, Anyfi::Address dstAddr, uint32_t scopeId) {
    ssize_t writeBytes;
    struct sockaddr_in6 sockaddress{};
    sockaddress.sin6_family = AF_INET6;
    sockaddress.sin6_port = htons(dstAddr.port());
    dstAddr.getIPv6AddrAsRawBytes((uint8_t *)&sockaddress.sin6_addr);
    sockaddress.sin6_scope_id = scopeId;

    if (buffer->isBackwardMode())
        writeBytes = sendto(sock, buffer->buffer(), buffer->getWritePos() - buffer->getReadPos(), 0, (struct sockaddr*)&sockaddress, sizeof(sockaddress));
    else
        writeBytes = sendto(sock, &buffer->buffer()[buffer->getReadPos()], buffer->getWritePos() - buffer->getReadPos(), 0, (struct sockaddr*)&sockaddress, sizeof(sockaddress));

    return (int)writeBytes;
}

void _closeImpl(int sock) {
    close(sock);
}
//
// Posix socket operation impl definition
// ========================================================================================
//


#endif