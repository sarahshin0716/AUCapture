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

#ifndef ANYFIMESH_CORE_WIFIP2PCHANNEL_H
#define ANYFIMESH_CORE_WIFIP2PCHANNEL_H

#include "../../Common/fsm.hpp"

#include "P2PChannel.h"

#define WIFIP2P_LINK_COUNT          5

#define WIFIP2P_HANDSHAKE_LINK      0
#define WIFIP2P_WRITE_LINK          1
#define WIFIP2P_READ_LINK           2
#define WIFIP2P_WRITE_V6LINK        3
#define WIFIP2P_READ_V6LINK         4

namespace Anyfi {
    class TimerTask;
}

class WifiP2PChannel : public P2PChannel {
public:
    WifiP2PChannel(unsigned short channelId, std::shared_ptr<Selector> selector, Anyfi::Address address, bool isGroupOwner)
            : P2PChannel(channelId, std::move(selector), std::move(address), WIFIP2P_LINK_COUNT), _isGroupOwner(isGroupOwner), readLink(-1), writeLink(-1) {
        _address.connectionType(Anyfi::ConnectionType::WifiP2P);
    }

    WifiP2PChannel(unsigned short channelId, std::shared_ptr<Link> link, Anyfi::Address address, bool isGroupOwner)
            : P2PChannel(channelId, link->selector(), std::move(address), WIFIP2P_LINK_COUNT), _isGroupOwner(isGroupOwner), readLink(-1), writeLink(-1) {
        _address.connectionType(Anyfi::ConnectionType::WifiP2P);
        _links[WIFIP2P_HANDSHAKE_LINK] = link;
    }
    ~WifiP2PChannel();

    void connect(Anyfi::Address address, const std::function<void(bool result)> &callback) override;
    bool open(Anyfi::Address address) override;
    void close() override;
    void write(NetworkBufferPointer buffer) override;

    void onRead(const std::shared_ptr<Link>& link, const NetworkBufferPointer &buffer) override;
    void onClose(std::shared_ptr<Link> closedLink) override;

    void doHandshake();
    void setP2PAcceptCallback(const std::function<void(std::shared_ptr<P2PChannel> p2pChannel)> &callback) { _p2pAcceptCallback = callback; }
private:
    bool _isGroupOwner = false;

    std::function<void(std::shared_ptr<P2PChannel> p2pChannel)> _p2pAcceptCallback = nullptr;
    std::function<void(bool result)> _p2pConnectCallback = nullptr;

    void sendHandshakeREQ(const std::vector<Anyfi::Address>& addresses);
    void sendHandshakeRES(const std::vector<Anyfi::Address>& addresses);
    void sendHandshakeLNK1();
    void sendHandshakeLNK2();
    void sendHandshakeLNK3();
    void processHandshakeREQ(NetworkBufferPointer buffer);
    void processHandshakeRES(NetworkBufferPointer buffer);
    void processHandshakeLNK(NetworkBufferPointer buffer);

    const std::vector<Anyfi::Address> openReadLink();
    void connectWriteLink(const std::vector<Anyfi::Address>& addresses);

private:
    enum P2PStatus : uint8_t {
        P2PINIT,
        P2PACCEPTING,
        P2PCONNECTING,
    };

    P2PStatus _state;

    enum LinkCheckState : uint8_t {
        LC_INIT,
        LC_RCHECK,
        LC_WCHECK,
        LC_LNK12,
        LC_LNK3,
        LC_DONE,
    };
    LinkCheckState _linkCheckState;
    int _linkCheckStateTicks;

    Anyfi::Config::mutex_type _linkLock;
    bool _gotLnk1, _gotLnk2, _gotLnk3;

    int readLink, writeLink;
    bool selectCalled;
    bool receivedLnk;
    int linkCheckRetry;

    std::shared_ptr<Anyfi::TimerTask> linkCheckTimeoutTask;

    std::vector<int> readLinkCandidates, writeLinkCandidates;

    int linkCheckNumRetries;

    void startLinkCheck();
    void stopLinkCheck();

    void clearLinkCheckTimer();
    void onLinkCheckTimeout();

    void setLinkCheckState(LinkCheckState state);

    int getLinkNo(const std::shared_ptr<Link>& link);
    bool isRLinkNo(int linkNo);
    bool isWLinkNo(int linkNo);

    void writeRLinkTask(int linkNo);
    void writeWLinkTask(int linkNo);
    void selectLinksTask();
    void callbackTask();
};


#endif //ANYFIMESH_CORE_WIFIP2PCHANNEL_H
