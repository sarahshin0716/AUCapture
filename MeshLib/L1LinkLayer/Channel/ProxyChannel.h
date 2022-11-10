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

#ifndef ANYFIMESH_CORE_PROXYCHANNEL_H
#define ANYFIMESH_CORE_PROXYCHANNEL_H

#include <linux/tcp.h>
#include "../Channel/Channel.h"
#include "../../L1LinkLayer/Link/SocketLink.h"

#define PROXY_SOCKET_MIN_ELAPSE_MICRO_SEC 2000000

class ProxyChannel : public SimpleChannel {
public:
    ProxyChannel(unsigned short channelId, std::shared_ptr<Selector> selector)
            : SimpleChannel(channelId, std::move(selector)) {};

    ProxyChannel(unsigned short channelId, std::shared_ptr<Selector> selector, const Anyfi::Address &address)
            : SimpleChannel(channelId, std::move(selector), address) {};

    void connect(Anyfi::Address address, const std::function<void(bool result)> &callback) override;
    bool open(Anyfi::Address address) override;
    void close() override;
    void write(NetworkBufferPointer buffer) override;

    void onClose(std::shared_ptr<Link> link) override;

    void updateTCPInfo();
    std::shared_ptr<tcp_info> getTcpInfo();


    uint32_t getIpAddr() { return _ipAddr; }
    bool getPerfChanged() { return _perfChanged; }
    void setPerfChanged(bool perfChanged) { _perfChanged = perfChanged; }
    std::chrono::time_point<std::chrono::system_clock> getLastWriteTime() { return _lastWriteTime; }
    void setLastWriteTime(std::chrono::time_point<std::chrono::system_clock> lastWriteTime) { _lastWriteTime = lastWriteTime; }
    bool hasLastWriteTime() { return _lastWriteTime != std::chrono::time_point<std::chrono::system_clock>::max(); }
    void clearLastWriteTime() { _lastWriteTime = std::chrono::time_point<std::chrono::system_clock>::max(); }

    // TCP
    unsigned int getRtt() { return _rtt; }
    bool setRtt(unsigned int rtt) {
        bool changed = false;
        if (rtt > 0 && _rtt != rtt) {
            _rtt = rtt;
            _perfChanged = true;
            changed = true;
        }
        return changed;
    }
    unsigned int getRttVar() { return _rtt_var; }
    bool setRttVar(unsigned int rtt_var) {
        bool changed = false;
        if (rtt_var > 0 && _rtt_var != rtt_var) {
            _rtt_var = rtt_var;
            _perfChanged = true;
            changed = true;
        }
        return changed;
    }

private:
    uint32_t _ipAddr = 0;
    std::shared_ptr<SocketLink> _proxyLink = nullptr;

    bool _perfChanged = false;

    std::chrono::time_point<std::chrono::system_clock> _lastWriteTime =  std::chrono::time_point<std::chrono::system_clock>::max();

    // TCP Performance
    unsigned int _rtt = 0;
    unsigned int _rtt_var = 0;
};


#endif //ANYFIMESH_CORE_PROXYCHANNEL_H
