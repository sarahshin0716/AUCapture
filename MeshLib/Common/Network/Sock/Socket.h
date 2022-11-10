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

#ifndef ANYFIMESH_CORE_SOCKET_H
#define ANYFIMESH_CORE_SOCKET_H

#include "../../../Common/Network/Address.h"


namespace Anyfi {

/**
 * A socket connect result.
 */
enum class SocketConnectResult {
    CONNECTED = 0,
    CONNECT_FAIL = 1,
    CONNECTING = 2,
};


/**
 * Abstract socket class.
 *
 * Anyfi::Socket designed to support cross-platform with a single class.
 * You can use this socket class regardless of the platform.
 */
class Socket {
public:
	/**
	 * Create a new socket with the given address
	 */
    static int create(const Address &addr, bool blocking = false);

    /**
     * Connects a socket to the given address.
     * @param sock  socket id
     * @param addr  destination address
     * @param in    network interface name (only for IPv6)
     * @return SocketConnectResult
     */
    static SocketConnectResult connect(int sock, const Anyfi::Address &addr, const std::string &in = "");

	/**
	 * Gets a connect operation result of the non-blocking socket.
	 */
	static SocketConnectResult connectResult(int sock);

    /**
     * 할당 정보 확인
     */
//    static Anyfi::Address getAddress(int sock, Anyfi::AddrType type);

    /**
     * Opens a socket.
     */
    static bool open(int sock, const Address &addr, const std::string& in = "");

    /**
     * Accept a socket.
     */
    static int accept(int sock);

    /**
     * Reads a socket.
     */
    static int read(int sock, NetworkBufferPointer buffer);

    /**
     * Reads a socket with limit
     */
    static int read(int sock, NetworkBufferPointer buffer, size_t limit);

    /**
     * Writes a given buffer to the socket.
     */
    static int write(int sock, NetworkBufferPointer buffer);

    /**
     * Send a given buffer to the socket.
     */
    static int send(int sock, NetworkBufferPointer buffer);
    /**
     * 소켓을 통해 dstAddr로 버퍼 전송. IPv6는 scopeId 필요
     */
    static int sendTo(int sock, NetworkBufferPointer buffer, Anyfi::Address dstAddr, int scopeId = -1);
    /**
     * Closes a socket.
     */
    static void close(int sock);

    /**
     * Get socket address.
     */
    static Anyfi::Address getSockAddress(int sock);

};

}   // namespace Anyfi

#endif //ANYFIMESH_CORE_SOCKET_H
