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

#ifndef ANYFIMESH_CORE_VPNCHANNEL_H
#define ANYFIMESH_CORE_VPNCHANNEL_H

#include <utility>
#include <thread>

#include "../../../L1LinkLayer/Channel/Channel.h"


// Forward declarations : IOTunnel.h
class IOTunnel;


class VpnChannel : public BaseChannel {
public:

    VpnChannel(unsigned short channelId)
            : BaseChannel(channelId, nullptr), _isVpnThreadRunning(false) {}
    VpnChannel(unsigned short channelId, std::shared_ptr<IOTunnel> ioTunnel)
            : BaseChannel(channelId, nullptr), _ioTunnel(std::move(ioTunnel)), _isVpnThreadRunning(false) {}

    ~VpnChannel();

    void connect(Anyfi::Address address, const std::function<void(bool result)> &callback) override {
        throw std::runtime_error("connect method is disabled in VpnChannel");
    }
    bool open(Anyfi::Address address) override;
    void close() override;
    void write(NetworkBufferPointer buffer) override;

    void onClose(std::shared_ptr<Link> link) override;

    std::vector<int> socks() override;

    /**
     * Protects a given sock from the vpn connection.
     */
    void protect(int sock);

private:
    unsigned short _tunFd = 0;
    std::shared_ptr<IOTunnel> _ioTunnel = nullptr;

    Anyfi::Config::mutex_type _writeQueueMutex;
    std::vector<NetworkBufferPointer> _writeQueue;
    void _addToWriteQueue(NetworkBufferPointer buffer);

    // VPN start / stop mutex
    Anyfi::Config::mutex_type _vpnMutex;
    std::atomic_bool _isVpnThreadRunning;
    std::unique_ptr<std::thread> _vpnReadThread = nullptr;
    std::unique_ptr<std::thread> _vpnWriteThread = nullptr;
    void _vpnReadThreadImpl();
    void _vpnWriteThreadImpl();
    void _startVpnThread();
    void _startVpnThreadWithoutLock();
    void _stopVpnThread();
    void _stopVpnThreadWithoutLock();

    int _doWrite(NetworkBufferPointer buffer);
    NetworkBufferPointer _doRead();
};


#endif //ANYFIMESH_CORE_VPNCHANNEL_H
