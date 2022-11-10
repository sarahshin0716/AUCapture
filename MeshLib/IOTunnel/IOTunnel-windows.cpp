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

// IOTunnel-window.cpp should only be compiled on windows
#ifdef _WIN32


#include "IOTunnel.h"

#include <thread>

#include "Common/Network/TunTap/TUN.h"


class WindowsIOTunnel : public IOTunnel {
public:
    bool startVPN() override;
    void stopVPN() override;
    NetworkBufferPointer readVPN() override;
    void writeVPN(NetworkBufferPointer buffer) override;

private:
    TUN::VpnInfo vpnInfo;

    bool _running = false;
    std::thread _ioThread;
    void _ioThreadImpl();
};


bool WindowsIOTunnel::startVPN() {
    auto tun = TUN::open_tun("");
    if (tun <= 0)
        return false;

    vpnInfo.tun_fh = (HANDLE) tun;
    vpnInfo.read_pending = false;
    vpnInfo.overlapped.Offset = 0;
    vpnInfo.overlapped.OffsetHigh = 0;
    vpnInfo.overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    _ioThread = std::thread(&WindowsIOTunnel::_ioThreadImpl, this);

    return true;
}

void WindowsIOTunnel::stopVPN() {
    _running = false;
}

NetworkBufferPointer WindowsIOTunnel::readVPN() {
    return nullptr;
}

void WindowsIOTunnel::writeVPN(NetworkBufferPointer buffer) {

}

void WindowsIOTunnel::_ioThreadImpl() {
    _running = true;

    while (_running) {
        auto buffer = TUN::read_tun(&vpnInfo);
        if (buffer != nullptr) {
            buffer->printAsHex();
        }
        std::cerr << "null" << std::endl;
    }
}

#endif