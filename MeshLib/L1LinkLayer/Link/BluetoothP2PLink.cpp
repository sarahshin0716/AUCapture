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

#include "BluetoothP2PLink.h"


Anyfi::SocketConnectResult BluetoothP2PLink::doSockConnect(Anyfi::Address address, const std::string &ifname) {
    return Anyfi::SocketConnectResult::CONNECT_FAIL;
}

bool BluetoothP2PLink::finishSockConnect() {
    return false;
}

bool BluetoothP2PLink::doOpen(Anyfi::Address address, const std::string& in) {
    return false;
}

std::shared_ptr<Link> BluetoothP2PLink::doAccept() {
    return std::shared_ptr<Link>();
}

void BluetoothP2PLink::doClose() {

}

NetworkBufferPointer BluetoothP2PLink::doRead() {
    return NetworkBufferPointer();
}

int BluetoothP2PLink::doWrite(NetworkBufferPointer buffer) {
    return false;
}
