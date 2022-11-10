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

// VpnChannel-tun.cpp should only be compiled on tun
#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR | TARGET_OS_IPHONE
        #define iOS
    #elif TARGET_OS_MAC
        #define _TUN
    #endif
#elif defined(ANDROID) || defined(__ANDROID__)
    #include "VpnChannel-android.cpp"
#elif __linux__
    #define _TUN
#else
    // Undefined
#endif

#if defined(_TUN)


#include "VpnChannel.h"


bool VpnChannel::open(Anyfi::Address address) {
    return false;
}

void VpnChannel::close() {

}

void VpnChannel::onClose(std::shared_ptr<Link> link) {

}

void VpnChannel::write(NetworkBufferPointer buffer) {

}

void VpnChannel::protect(int sock) {

}

int VpnChannel::_doWrite(NetworkBufferPointer buffer) {

}

NetworkBufferPointer VpnChannel::_doRead() {
    return NetworkBufferPointer();
}




#endif

//void InternalVpnLink::protect(int sock) {
//    TUN::bind_to_interface(sock, _originalGateway.c_str());
//}
//
//bool InternalVpnLink::doOpen(Anyfi::Address address) {
//    _originalGateway = TUN::get_default_interface();
//    if (_originalGateway.empty())
//        std::cerr << "Failed to get original gateway" << std::endl;
//
//    _sock = TUN::open_tun("tun0");
//
//    TUN::set_address("tun0", "10.0.0.1", 0);
//
//    TUN::add_route("10.0.0.1", "0.0.0.0", 1);           /* Add 00000000.00000000.00000000.00000000/1 */
//    TUN::add_route("10.0.0.1", "128.0.0.0", 2);         /* Add 10000000.00000000.00000000.00000000/2 */
//    TUN::add_route("10.0.0.1", "240.0.0.0", 4);         /* Add 11110000.00000000.00000000.00000000/4 */
//    TUN::add_route("10.0.0.1", "208.0.0.0", 4);         /* Add 11010000.00000000.00000000.00000000/4 */
//    TUN::add_route("10.0.0.1", "200.0.0.0", 5);         /* Add 11001000.00000000.00000000.00000000/5 */
//    TUN::add_route("10.0.0.1", "196.0.0.0", 6);         /* Add 11000100.00000000.00000000.00000000/6 */
//    TUN::add_route("10.0.0.1", "194.0.0.0", 7);         /* Add 11000010.00000000.00000000.00000000/7 */
//    TUN::add_route("10.0.0.1", "193.0.0.0", 8);         /* Add 11000001.00000000.00000000.00000000/8 */
//    TUN::add_route("10.0.0.1", "192.0.0.0", 9);         /* Add 11000000.00000000.00000000.00000000/9 */
//    TUN::add_route("10.0.0.1", "192.192.0.0", 10);      /* Add 11000000.11000000.00000000.00000000/10 */
//    TUN::add_route("10.0.0.1", "192.128.0.0", 11);      /* Add 11000000.10000000.00000000.00000000/11 */
//    TUN::add_route("10.0.0.1", "192.176.0.0", 12);      /* Add 11000000.10110000.00000000.00000000/12 */
//    TUN::add_route("10.0.0.1", "192.160.0.0", 13);      /* Add 11000000.10100000.00000000.00000000/13 */
//    TUN::add_route("10.0.0.1", "192.172.0.0", 14);      /* Add 11000000.10101100.00000000.00000000/14 */
//    TUN::add_route("10.0.0.1", "192.170.0.0", 15);      /* Add 11000000.10101010.00000000.00000000/15 */
//    TUN::add_route("10.0.0.1", "192.169.0.0", 16);      /* Add 11000000.10101001.00000000.00000000/16 */
//    TUN::add_route("10.0.0.1", "192.168.128.0", 17);    /* Add 11000000.10101000.10000000.00000000/17 */
//    TUN::add_route("10.0.0.1", "192.168.64.0", 18);     /* Add 11000000.10101000.01000000.00000000/18 */
//    TUN::add_route("10.0.0.1", "192.168.0.0", 19);      /* Add 11000000.10101000.00000000.00000000/19 */
//    TUN::add_route("10.0.0.1", "192.168.32.0", 20);     /* Add 11000000.10101000.00100000.00000000/20 */
//    TUN::add_route("10.0.0.1", "192.168.56.0", 21);     /* Add 11000000.10101000.00111000.00000000/21 */
//    TUN::add_route("10.0.0.1", "192.168.52.0", 22);     /* Add 11000000.10101000.00110100.00000000/22 */
//    TUN::add_route("10.0.0.1", "192.168.50.0", 23);     /* Add 11000000.10101000.00110010.00000000/23 */
//    TUN::add_route("10.0.0.1", "192.168.48.0", 24);     /* Add 11000000.10101000.00110000.00000000/24 */
//
//    state(LinkState::CONNECTED);
//
//    registerOperation(_selector, SelectionKeyOp::OP_READ);
//    return true;
//}
//
//void InternalVpnLink::doClose() {
//    Anyfi::Socket::close(_sock);
//}
//
//NetworkBufferPointer InternalVpnLink::doRead() {
//    return _readFromSock(_sock);
//}
//
//void InternalVpnLink::doWrite(NetworkBufferPointer buffer) {
//    Anyfi::Socket::write(_sock, std::move(buffer));
//}