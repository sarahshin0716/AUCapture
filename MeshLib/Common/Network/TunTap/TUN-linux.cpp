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

// TUN-linux.cpp should only be compiled on linux
#ifdef __linux__

#include "TUN.h"


int TUN::open_tun(std::string name) {
    return 0;
}

int TUN::set_address(std::string name, std::string addr, int prefix) {
    return 0;
}

void TUN::add_route(std::string name, std::string route, int prefix) {

}

void TUN::set_default_route(std::string route) {

}

std::string TUN::get_default_interface() {
    return "";
}

void TUN::set_default_interface(const std::string &intf) {

}

int TUN::bind_to_interface(int socket, const char *name) {
    return 0;
}


#endif