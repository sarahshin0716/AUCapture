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

// TUN-darwin should only be compiled on darwin
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_MAC


#include "TUN.h"

#include "Common/System/System.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <regex>


inline in_addr_t *as_in_addr(sockaddr *sa) {
    return &((sockaddr_in *) sa)->sin_addr.s_addr;
}


int TUN::open_tun(std::string name) {
    int fd = open("/dev/tun0", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << strerror(errno) << std::endl;
    }

    ifreq ifr{};
    memset(&ifr, 0, sizeof(ifr));

    // Configure interface name if it is specified
    if (name != "") {
        strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    }
    return fd;
}

int TUN::set_address(std::string name, std::string addr, int prefix) {
    int result = 0;
    if (strchr(addr.c_str(), ':')) {
        // TODO : Add support IPv6
    } else {
        // Add an IPv4 address.
        ifreq ifr4;
        memset(&ifr4, 0, sizeof(ifr4));
        strncpy(ifr4.ifr_name, name.c_str(), IFNAMSIZ);
        ifr4.ifr_addr.sa_family = AF_INET;

        struct sockaddr_in sin{};
        memset(&sin, 0, sizeof(struct sockaddr));
        sin.sin_family = AF_INET;
        sin.sin_port = 0;
        sin.sin_addr.s_addr = inet_addr(addr.c_str());
        memcpy(&ifr4.ifr_addr, &sin, sizeof(struct sockaddr));

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (ioctl(sock, SIOCSIFADDR, &ifr4)) {
            result = (errno == EINVAL) ? TUN_ERR_BAD_ARGUMENT : TUN_ERR_SYSTEM_ERROR;
        }
        close(sock);
    }

    if (result == TUN_ERR_BAD_ARGUMENT)
        std::cerr << "Invalid address: " << addr << "/" << prefix << std::endl;
    else if (result == TUN_ERR_SYSTEM_ERROR)
        std::cerr << "Cannot set address: " << addr << "/" << prefix << ": " << strerror(errno) << std::endl;

    return result;
}

void TUN::add_route(std::string name, std::string route, int prefix) {
    std::string cmd("route learn ");
    cmd += route + "/" + to_string(prefix);
    cmd += " " + name;

    System::shell(cmd.c_str());
}

void TUN::set_default_route(std::string route) {
    System::shell("route delete default");

    std::string cmd("route learn default ");
    cmd += route;
    System::shell(cmd.c_str());
}

void TUN::set_default_interface(const std::string &intf) {
    std::string cmd("route change default -interface ");
    cmd += intf;
    System::shell(cmd.c_str());
}

std::string TUN::get_default_interface() {
    std::string result = System::shell("route get default");
    std::regex reg("interface: .+");
    result = std::sregex_iterator(result.begin(), result.end(), reg)->str();
    result.erase(result.begin(), result.begin() + 11);
    return result;
}

int TUN::bind_to_interface(int socket, const char *name) {
    struct ifreq ifr{};
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name) - 1);
    struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
    sin->sin_family = AF_INET;  // TOOD ipv6

    if(ioctl(socket, SIOCSIFADDR, &ifr) < 0){
        std::cerr << "Cannot bind socket to " << name << ": " << strerror(errno) << std::endl;
        return TUN_ERR_SYSTEM_ERROR;
    }
    return 0;


//    struct ifreq ifr;
//    memset(&ifr, 0, sizeof(struct ifreq));
//    sprintf(ifr.ifr_name, name);
//
//    if (setsockopt(socket, SIOCSIFADDR, IP_RECVIF, name, strlen(name))) {
//        std::cerr << "Cannot bind socket to " << name << ": " << strerror(errno) << std::endl;
//        return TUN_ERR_SYSTEM_ERROR;
//    }
//    return 0;
}


#endif
#endif