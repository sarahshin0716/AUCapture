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

// Socket-windows.cpp should only be compiled on windows.
#ifdef _WIN32

#include "Socket.h"

#pragma comment (lib, "Ws2_32.lib")
#include <winsock2.h>
#include <WS2tcpip.h>

#include "Log/Frontend/Log.h"


//
// ========================================================================================
// Posix socket operation impl declaration
//
Anyfi::SocketConnectResult _connectImpl(int sock, const Anyfi::Address &address);

bool _openImpl(int sock, const Anyfi::Address &address);

int _acceptImpl(int sock);

int _readImpl(int sock, NetworkBufferPointer buffer);

bool _writeImpl(int sock, NetworkBufferPointer buffer);

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
            throw std::invalid_argument("Invalid address type.");
    }

    int type = 0;
    int protocol = 0;
    switch (addr.connectionProtocol()) {
        case Anyfi::ConnectionProtocol::TCP:
            type = SOCK_STREAM;
            protocol = IPPROTO_TCP;
            break;
        case Anyfi::ConnectionProtocol::UDP:
        case Anyfi::ConnectionProtocol::MeshUDP:
        case Anyfi::ConnectionProtocol::MeshRUDP:
            type = SOCK_DGRAM;
            protocol = IPPROTO_UDP;
            break;
        default:
            throw std::invalid_argument("Invalid connection protocol");
    }

    int sock = socket(domain, type, protocol);
    if (sock == INVALID_SOCKET) {
        Anyfi::Log::error(__func__, "Failed to create socket");
        close(sock);
        return -1;
    }
    if (!blocking) {
        // Configure non-blocking
        u_long nonBlocking = 1;
        if (ioctlsocket(sock, FIONBIO, &nonBlocking) != NO_ERROR) {
            Anyfi::Log::error(__func__, "Failed to configure socket to non-blocking");
            close(sock);
            return -1;
        }
    }

	return sock;
}

Anyfi::SocketConnectResult Anyfi::Socket::connect(int sock, const Anyfi::Address &address) {
	return _connectImpl(sock, address);
}

Anyfi::SocketConnectResult Anyfi::Socket::connectResult(int sock) {
    int err = 0;
    int errLen = sizeof(err);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *) &err, &errLen) < 0) {
        // Get socket option failed
        return Anyfi::SocketConnectResult::CONNECT_FAIL;
    }

    if (err == 0)
        return Anyfi::SocketConnectResult::CONNECTED;

    err = WSAGetLastError();
    if (err == WSAECONNREFUSED) {
        // Connect refused
    } else if (err == WSAETIMEDOUT) {
        // Socket-connect timeout a kernel level
    }
    return Anyfi::SocketConnectResult::CONNECT_FAIL;
}

bool Anyfi::Socket::open(int sock, const Anyfi::Address &addr) {
	return _openImpl(sock, addr);
}

int Anyfi::Socket::accept(int sock) {
    return _acceptImpl(sock);
}

int Anyfi::Socket::read(int sock, NetworkBufferPointer buffer) {
	return _readImpl(sock, std::move(buffer));
}

bool Anyfi::Socket::write(int sock, NetworkBufferPointer buffer) {
	return _writeImpl(sock, std::move(buffer));
}

void Anyfi::Socket::close(int sock) {
	_closeImpl(sock);
}

Anyfi::Address Anyfi::Socket::getSockAddress(int sock) {
    sockaddr_in sin;
    int len = sizeof(sin);
    if (getsockname(sock, (struct sockaddr *) &sin, &len) < 0)
        return Anyfi::Address();

    Anyfi::Address addr;
    char buf[46];
    inet_ntop(sin.sin_family, &sin.sin_addr, buf, sizeof(buf));
    addr.port(ntohs(sin.sin_port));

    if (sin.sin_family == AF_INET) {
        addr.type(Anyfi::AddrType::IPv4);
        addr.setIPv4Addr(ntohl(sin.sin_addr.s_addr));
    } else if (sin.sin_family == AF_INET6) {
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
Anyfi::SocketConnectResult _connectImpl(int sock, const Anyfi::Address &address) {
    int result = 0;
    if (address.type() == Anyfi::AddrType::IPv4) {  // IPv4 socket connect
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(address.port());

        result = connect(sock, (SOCKADDR *) &addr, sizeof(addr));
    } else {    // IPv6 socket connect
        result = -1;    // TODO : Support IPv6
    }

    if (result == 0) {
        // The connection is established immediately when connect to local address.
        return Anyfi::SocketConnectResult::CONNECTED;
    }
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
        // Connect failed
        Anyfi::Log::error(__func__, "Socket connect failed : " + WSAGetLastError());
        return Anyfi::SocketConnectResult::CONNECT_FAIL;
    }

	return Anyfi::SocketConnectResult::CONNECTING;
}

bool _openImpl(int sock, const Anyfi::Address &address) {
    if (address.type() == Anyfi::AddrType::IPv4) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port());
        addr.sin_addr.s_addr = inet_addr(address.addr().c_str());

        if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            Anyfi::Log::error(__func__, "Socket bind failed : " + WSAGetLastError());
            return false;
        }

        if (address.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            if (listen(sock, 10) < 0) {
                Anyfi::Log::error(__func__, "Socket listen failed : " + WSAGetLastError());
                return false;
            }
        }

        return true;
    }
	return false;
}

int _acceptImpl(int sock) {
    return accept(sock, nullptr, nullptr);
}

int _readImpl(int sock, NetworkBufferPointer buffer) {
    int readBytes = read(sock, buffer->buffer(), buffer->capacity());

    if (readBytes > 0)
        buffer->setWritePos((uint32_t) readBytes);

	return readBytes;
}

bool _writeImpl(int sock, NetworkBufferPointer buffer) {
    int writeBytes;
    if (buffer->isBackwardMode())
        writeBytes = write(sock, buffer->buffer(), buffer->getWritePos() - buffer->getReadPos());
    else
        writeBytes = write(sock, &buffer->buffer()[buffer->getReadPos()],
            buffer->getWritePos() - buffer->getReadPos());

    if (writeBytes < 0) {
        Anyfi::Log::error(__func__, "Write failed : " + WSAGetLastError());
        return -1;
    }

    buffer->setReadPos(buffer->getReadPos() + writeBytes);
	return writeBytes;
}

void _closeImpl(int sock) {
	close(sock);
}
//
// Posix socket operation impl definition
// ========================================================================================
//


#endif