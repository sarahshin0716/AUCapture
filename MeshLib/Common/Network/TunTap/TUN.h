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

#ifndef ANYFIMESH_CORE_TUN_H
#define ANYFIMESH_CORE_TUN_H

#include <string>

#include "Common/Network/Buffer/NetworkBuffer.h"


#define TUN_ERR_SYSTEM_ERROR (-1)
#define TUN_ERR_BAD_ARGUMENT (-2)


#ifdef _WIN32
#include <Windows.h>
#endif


/**
 * In order to use TUN in darwin, you need to pre-install tuntaposx.
 * > TuntapOSX : http://tuntaposx.sourceforge.net/
 * In order to use TUN in windows, you need to pre-install o
 */
namespace TUN {

/**
 * Open and allocate TUN device.
 *
 * @param name
 */
int open_tun(std::string name);

/**
 * Assign a IP address to network interface
 *
 * @param name Name of network interface.
 * @param addr IP address to assign
 * @param prefix Prefix of ip address.
 */
int set_address(std::string name, std::string addr, int prefix);

/**
 * Add route.
 *
 * @param name Gateway address if darwin OS, tun interface name otherwise.
 * @param route Destination ip address.
 * @param prefix Prefix of destination ip address.
 */
void add_route(std::string name, std::string route, int prefix);

/**
 * Set default gateway route.
 */
void set_default_route(std::string route);

/**
 * Gets a name of the default gateway interface.
 */
std::string get_default_interface();

/**
 * Set default interface
 */
void set_default_interface(const std::string &intf);

/**
 * Binds a socket to the interface.
 *
 * @param socket Socket to bind
 * @param name The interface name
 */
int bind_to_interface(int socket, const char *name);


#ifdef _WIN32

/**
 * Network interface stats
 */
struct NIStats {
    uint64_t tx_pkts;
    uint64_t tx_bytes;
    uint64_t rx_pkts;
    uint64_t rx_bytes;
};
struct VpnInfo {
    HANDLE tun_fh;
    uint32_t mtu;
    OVERLAPPED overlapped;
    bool read_pending;
    NIStats stats;
};

/**
 * Reads a buffer from the tun.
 */
NetworkBufferPointer read_tun(VpnInfo *info);

/**
 * Writes a given buffer to the tun.
 */
void write_tun(VpnInfo *info, NetworkBufferPointer buffer);

#endif

}   // namespace TUN

#endif //ANYFIMESH_CORE_TUN_H
