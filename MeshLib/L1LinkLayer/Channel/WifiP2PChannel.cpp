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

#include "../../Common/Network/Buffer/NetworkBufferPool.h"
#include "../../L2NetworkLayer/Mesh/MeshPacket.h"
#include "../../Core.h'
#include "../../Log/Backend/LogManager.h"
#include "../../Log/Frontend/Log.h"
#include "../../L1LinkLayer/Channel/Packet/P2PLinkHandshakePacket.h"
#include "../../L1LinkLayer/Channel/Packet/P2PLinkLinkCheckPacket.h"
#include "../../L1LinkLayer/Link/SocketLink.h"
#include "../../Common/Timer.h"
#include "WifiP2PChannel.h"

// TODO : Android dependent 요소는 WifiP2PChannel-andorid.cpp로 분리
// 현재는 WifiP2PChannel은 안드로이드에서만 사용한다고 가정하고 구현되어 있음.
//#if defined(ANDROID) || defined(__ANDROID__)
#define UPLINK_INF_NAME     "wlan0"
#define DOWNLINK_INF_NAME   "p2p-wlan0-0" // TODO: 192.168.49.1 ip를 가진 디바이스
#define FIXED_GO_IP         "192.168.49.1"
#define WIFIP2P_IP          "192.168.49."
//#else
//
//#define UPLINK_INF_NAME     "en1"
//#define DOWNLINK_INF_NAME   "en1"
//#endif // #if defined(ANDROID) || defined(__ANDROID__)

WifiP2PChannel::~WifiP2PChannel(){
    stopLinkCheck();
}

void WifiP2PChannel::connect(Anyfi::Address address, const std::function<void(bool result)> &callback) {
    //Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2, "WifiP2PChannel::connect", "threadId=" + thread_id_str());

    _state = P2PINIT;
    startLinkCheck();

    state(State::CONNECTING);
    _p2pConnectCallback = callback;

    _links[WIFIP2P_HANDSHAKE_LINK] = std::make_shared<SocketLink>(_selector, shared_from_this());

    // TODO: wifi p2p link connect failed
    _links[WIFIP2P_HANDSHAKE_LINK]->connect(address, [this](bool success) {
        Anyfi::LogManager::getInstance()->record()->wifiP2PHandshakeLinkConnect(success);
        if (!success) {
            _p2pConnectCallback(false);
            _p2pConnectCallback = nullptr;
            return;
        }
        doHandshake();
    });
}


bool WifiP2PChannel::open(Anyfi::Address address) {
    //Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2, "WifiP2PChannel::open", "threadId=" + thread_id_str());

    return false;
}


void WifiP2PChannel::close() {
//    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2, "WifiP2PChannel::close", "threadId=" + thread_id_str());
    stopLinkCheck();

    for (auto &link : _links) {
        if (link) {
            link->closeWithoutNotify();
            link = nullptr;
        }
    }
    state(CLOSED);
}

void WifiP2PChannel::onClose(std::shared_ptr<Link> closedLink) {
    //Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2, "WifiP2PChannel::onClose", "threadId=" + thread_id_str());
    if (_links[WIFIP2P_HANDSHAKE_LINK] == closedLink) {
        _links[WIFIP2P_HANDSHAKE_LINK] = nullptr;
        if (state() == State::CONNECTING) {
            // handshake failed;
            if (_links[WIFIP2P_READ_LINK]) {
                _links[WIFIP2P_READ_LINK]->closeWithoutNotify();
                _links[WIFIP2P_READ_LINK] = nullptr;
            }
            if (_links[WIFIP2P_WRITE_LINK]) {
                _links[WIFIP2P_WRITE_LINK]->closeWithoutNotify();
                _links[WIFIP2P_WRITE_LINK] = nullptr;
            }
            if (_links[WIFIP2P_READ_V6LINK]) {
                _links[WIFIP2P_READ_V6LINK]->closeWithoutNotify();
                _links[WIFIP2P_READ_V6LINK] = nullptr;
            }
            if (_links[WIFIP2P_WRITE_V6LINK]) {
                _links[WIFIP2P_WRITE_V6LINK]->closeWithoutNotify();
                _links[WIFIP2P_WRITE_V6LINK] = nullptr;
            }
            state(CLOSED);

            // _links[WIFIP2P_HANDSHAKE_LINK]->connect에서 올라가는 콜백과 중복되는 경우 확인
            if (_closeCallback)
                _closeCallback(shared_from_this());

            stopLinkCheck();
        } else if (state() == State::CONNECTED || state() == State::CLOSED) {
            // 무시해도 됨
        }
    } else {
        if (_links[WIFIP2P_READ_LINK] == closedLink) {
            Anyfi::Log::error("WifiP2PChannel", "read link closed");
        } else if (_links[WIFIP2P_WRITE_LINK] == closedLink) {
            Anyfi::Log::error("WifiP2PChannel", "write link closed");
        } else if (_links[WIFIP2P_READ_V6LINK] == closedLink) {
            Anyfi::Log::error("WifiP2PChannel", "read v6 link closed");
        } else if (_links[WIFIP2P_WRITE_V6LINK] == closedLink) {
            Anyfi::Log::error("WifiP2PChannel", "write v6 link closed");
        }
        for (auto &link : _links) {
            if (link != nullptr && link != closedLink) {
                link->closeWithoutNotify();
            }
            link = nullptr;
        }

        state(CLOSED);
        if (_closeCallback)
            _closeCallback(shared_from_this());
    }
}

void WifiP2PChannel::write(NetworkBufferPointer buffer) {
    //Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2, "WifiP2PChannel::write", "threadId=" + thread_id_str());

    if (writeLink == -1) {
        NetworkBufferPointer bufferCopy = NetworkBufferPool::acquire();
        bufferCopy->copyFrom(buffer.get());

        if (_links[WIFIP2P_WRITE_LINK]->state() == LinkState::CONNECTED)
            _links[WIFIP2P_WRITE_LINK]->write(buffer);
        if (_links[WIFIP2P_WRITE_V6LINK]->state() == LinkState::CONNECTED)
            _links[WIFIP2P_WRITE_V6LINK]->write(bufferCopy);
    } else {
        if (_links[writeLink]->state() == LinkState::CONNECTED)
            _links[writeLink]->write(buffer);
    };
}

void WifiP2PChannel::onRead(const std::shared_ptr<Link> &link, const NetworkBufferPointer &buffer) {
    {
        P2PLinkLinkCheckPacket::Type packetType = static_cast<P2PLinkLinkCheckPacket::Type>(buffer->peek<uint8_t>());
        switch (packetType) {
            case P2PLinkLinkCheckPacket::RLinkCheck: {
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                                  "Get MeshPacket::RLinkCheck from " + link->addr().addr() + ":" +
                                  to_string(link->addr().port()));

                std::shared_ptr<P2PLinkLinkCheckPacket> packet(std::make_shared<P2PLinkLinkCheckPacket>(buffer));

                int wlinkNo = packet->_link;

                int linkNo = -1;
                if (link == _links[WIFIP2P_READ_V6LINK]) {
                    linkNo = WIFIP2P_READ_V6LINK;
                } else if (link == _links[WIFIP2P_READ_LINK]) {
                    linkNo = WIFIP2P_READ_LINK;
                }
                if (linkNo == -1) {
                    // ERROR
                    return;
                }

                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                                  "**** my RL " + to_string(linkNo) + " other WL " + to_string(wlinkNo) + " ");
                writeWLinkTask(wlinkNo);
                readLinkCandidates.push_back(linkNo);
                break;
            }
            case P2PLinkLinkCheckPacket::WLinkCheck: {
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                                  "Get MeshPacket::WLinkCheck from " + link->addr().addr() + ":" +
                                  to_string(link->addr().port()));

                std::shared_ptr<P2PLinkLinkCheckPacket> packet(std::make_shared<P2PLinkLinkCheckPacket>(buffer));

                int wlinkNo = packet->_link;

                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__, "**** my available WL " + to_string(wlinkNo));
                writeLinkCandidates.push_back(wlinkNo);

                int linkNo = -1;
                if (link == _links[WIFIP2P_READ_V6LINK]) {
                    linkNo = WIFIP2P_READ_V6LINK;
                } else if (link == _links[WIFIP2P_READ_LINK]) {
                    linkNo = WIFIP2P_READ_LINK;
                }
                if (linkNo == -1) {
                    // ERROR
                    return;
                }
                readLinkCandidates.push_back(linkNo);

                break;
            }
            default:
                break;
        }
    }

    if (state() == State::CONNECTED) {
        if (_readCallback)
            _readCallback(shared_from_this(), link, buffer);
    } else if (state() == State::CONNECTING) {
        auto type = P2PLinkHandshakePacket::getType(buffer);
        if (type == P2PLinkHandshakePacket::Type::REQ) {
            _state = P2PACCEPTING;
            processHandshakeREQ(buffer);
        } else if (type == P2PLinkHandshakePacket::Type::RES) {
            _state = P2PCONNECTING;
            processHandshakeRES(buffer);
        } else if (type == P2PLinkHandshakePacket::Type::LNK1 || type == P2PLinkHandshakePacket::Type::LNK2 ||
                   type == P2PLinkHandshakePacket::Type::LNK3) {
            processHandshakeLNK(buffer);
        }
    }
}

const std::vector<Anyfi::Address> WifiP2PChannel::openReadLink() {
    std::vector<Anyfi::Address> addresses;

    std::string ifname = _isGroupOwner ? Anyfi::Address::getInterfaceNameWithIPv4Addr(FIXED_GO_IP)
                                       : Anyfi::Address::getInterfaceNameContainsIPv4Addr(WIFIP2P_IP);

    // v4 link
    {
        auto address = Anyfi::Address();
        address.connectionProtocol(Anyfi::ConnectionProtocol::UDP);
        address.type(Anyfi::AddrType::IPv4);
        address.addr(Anyfi::Address::getIPv4AddrFromInterface(ifname).addr());
        address.port(0);
        _links[WIFIP2P_READ_LINK] = std::make_shared<SocketLink>(_selector, shared_from_this());
        _links[WIFIP2P_READ_LINK]->open(address, ifname);

        auto assignedAddr = Anyfi::Socket::getSockAddress(_links[WIFIP2P_READ_LINK]->sock());
        address.port(assignedAddr.port());

        addresses.push_back(address);
        Anyfi::LogManager::getInstance()->record()->wifiP2POpenReadLink(address);
    }

    // v6 link
    {
        auto address = Anyfi::Address();
        address.connectionProtocol(Anyfi::ConnectionProtocol::UDP);
        address.type(Anyfi::AddrType::IPv6);
        address.port(0);
        _links[WIFIP2P_READ_V6LINK] = std::make_shared<SocketLink>(_selector, shared_from_this());
        _links[WIFIP2P_READ_V6LINK]->open(address, ifname);

        auto assignedAddr = Anyfi::Socket::getSockAddress(_links[WIFIP2P_READ_V6LINK]->sock());
        address.port(assignedAddr.port());
        address.addr(Anyfi::Address::getIPv6AddrFromInterface(ifname).addr());

        addresses.push_back(address);
        Anyfi::LogManager::getInstance()->record()->wifiP2POpenReadLink(address);
    }
    return addresses;
}

void WifiP2PChannel::doHandshake() {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__, "()");
    state(CONNECTING);
    if (_isGroupOwner) {
        _state = P2PINIT;
        startLinkCheck();

        _links[WIFIP2P_HANDSHAKE_LINK]->registerOperation(_selector, SelectionKeyOp::OP_READ);
    } else {
        auto readAddresses = openReadLink();

        for (auto it = readAddresses.begin(); it != readAddresses.end(); it++) {
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                              "openReadLink: " + it->addr() + ":" + to_string(it->port()));
        }
        sendHandshakeREQ(readAddresses);

        setLinkCheckState(LC_RCHECK);
    }
}

void WifiP2PChannel::sendHandshakeREQ(const std::vector<Anyfi::Address> &addresses) {
    for (auto it = addresses.begin(); it != addresses.end(); it++) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__, "- " + it->addr() + ":" + to_string(it->port()));
    }
    Anyfi::LogManager::getInstance()->record()->wifiP2PHandshakeWrite(P2PLinkHandshakePacket::Type::REQ);
    auto reqPacket = std::make_shared<P2PLinkHandshakePacket>(P2PLinkHandshakePacket::Type::REQ, addresses);
    _links[WIFIP2P_HANDSHAKE_LINK]->write(reqPacket->toPacket());
}

void WifiP2PChannel::sendHandshakeRES(const std::vector<Anyfi::Address> &addresses) {
    for (auto it = addresses.begin(); it != addresses.end(); it++) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__, "- " + it->addr() + ":" + to_string(it->port()));
    }
    Anyfi::LogManager::getInstance()->record()->wifiP2PHandshakeWrite(P2PLinkHandshakePacket::Type::RES);
    auto resPacket = std::make_shared<P2PLinkHandshakePacket>(P2PLinkHandshakePacket::Type::RES, addresses);
    _links[WIFIP2P_HANDSHAKE_LINK]->write(resPacket->toPacket());
}

void WifiP2PChannel::sendHandshakeLNK1() {
    auto resPacket = std::make_shared<P2PLinkHandshakePacket>(P2PLinkHandshakePacket::Type::LNK1,
                                                              std::vector<Anyfi::Address>());
    _links[WIFIP2P_HANDSHAKE_LINK]->write(resPacket->toPacket());
}

void WifiP2PChannel::sendHandshakeLNK2() {
    auto resPacket = std::make_shared<P2PLinkHandshakePacket>(P2PLinkHandshakePacket::Type::LNK2,
                                                              std::vector<Anyfi::Address>());
    _links[WIFIP2P_HANDSHAKE_LINK]->write(resPacket->toPacket());
}

void WifiP2PChannel::sendHandshakeLNK3() {
    auto resPacket = std::make_shared<P2PLinkHandshakePacket>(P2PLinkHandshakePacket::Type::LNK3,
                                                              std::vector<Anyfi::Address>());

    _links[WIFIP2P_HANDSHAKE_LINK]->write(resPacket->toPacket());
}

void WifiP2PChannel::processHandshakeREQ(NetworkBufferPointer buffer) {
    setLinkCheckState(LC_RCHECK);

    Anyfi::LogManager::getInstance()->record()->wifiP2PHandshakeRead(P2PLinkHandshakePacket::Type::REQ);
    auto reqPacket = std::make_shared<P2PLinkHandshakePacket>(buffer);
    auto writeAddrs = reqPacket->addrs();
    connectWriteLink(writeAddrs);

    auto readAddresses = openReadLink();
    for (auto it = readAddresses.begin(); it != readAddresses.end(); it++) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                          "openReadLink: " + it->addr() + ":" + to_string(it->port()));
    }
    sendHandshakeRES(readAddresses);
}

void WifiP2PChannel::processHandshakeRES(NetworkBufferPointer buffer) {
    Anyfi::LogManager::getInstance()->record()->wifiP2PHandshakeRead(P2PLinkHandshakePacket::Type::RES);
    auto resPacket = std::make_shared<P2PLinkHandshakePacket>(buffer);
    auto writeAddrs = resPacket->addrs();
    connectWriteLink(writeAddrs);
}

void WifiP2PChannel::processHandshakeLNK(NetworkBufferPointer buffer) {
    auto resPacket = std::make_shared<P2PLinkHandshakePacket>(buffer);
    P2PLinkHandshakePacket::Type type = resPacket->type();

    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__, "got LNK" + to_string(type));

    if (type == P2PLinkHandshakePacket::Type::LNK1) {
        _gotLnk1 = true;
    } else if (type == P2PLinkHandshakePacket::Type::LNK2) {
        _gotLnk2 = true;
    } else if (type == P2PLinkHandshakePacket::Type::LNK3) {
        _gotLnk3 = true;
    }
}

void WifiP2PChannel::connectWriteLink(const std::vector<Anyfi::Address> &addresses) {
    std::string ifname = _isGroupOwner ? Anyfi::Address::getInterfaceNameWithIPv4Addr(FIXED_GO_IP)
                                       : Anyfi::Address::getInterfaceNameContainsIPv4Addr(WIFIP2P_IP);

    for (int i = 0; i < addresses.size(); i++) {
        auto addr = addresses[i];
        addr.connectionProtocol(Anyfi::ConnectionProtocol::UDP);
        addr.connectionType(Anyfi::ConnectionType::WifiP2P);

        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                          "(" + addr.addr() + ":" + to_string(addr.port()) + ") using " + ifname);

        int linkNo = WIFIP2P_WRITE_LINK;
        if (addr.type() == Anyfi::IPv6) {
            linkNo = WIFIP2P_WRITE_V6LINK;
        } else if (addr.type() == Anyfi::IPv4) {
            linkNo = WIFIP2P_WRITE_LINK;
        }

        _links[linkNo] = std::make_shared<SocketLink>(_selector, shared_from_this());
        _links[linkNo]->connect(addr, ifname, [this, linkNo, addr](bool results) {
            if (results) {
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "connectWriteLink",
                                  " success (" + addr.addr() + ":" + to_string(addr.port()) + ")");

                Anyfi::Timer::schedule([this, linkNo]{
                                           writeRLinkTask(linkNo);
                                       }
                        , 500);
            } else
                Anyfi::Log::error(__func__,
                                  "connectWriteLink failed (" + addr.addr() + ":" + to_string(addr.port()) + ")");
        });
    }
}

int WifiP2PChannel::getLinkNo(const std::shared_ptr<Link> &link) {
    for (int i = 0; i < WIFIP2P_LINK_COUNT; i++) {
        if (_links[i] == link)
            return i;
    }
    return -1;
}

bool WifiP2PChannel::isRLinkNo(int linkNo) {
    return linkNo == WIFIP2P_READ_LINK || linkNo == WIFIP2P_READ_V6LINK;
}

bool WifiP2PChannel::isWLinkNo(int linkNo) {
    return linkNo == WIFIP2P_WRITE_LINK || linkNo == WIFIP2P_WRITE_V6LINK;
}

void WifiP2PChannel::writeRLinkTask(int linkNo) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "writeRLinkTask", "");

    if(_links[linkNo] == nullptr) {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "writeRLinkTask", "nullptr");
        return;
    }

    std::shared_ptr<P2PLinkLinkCheckPacket> packet = std::make_shared<P2PLinkLinkCheckPacket>(P2PLinkLinkCheckPacket::RLinkCheck, (uint8_t) linkNo);
    NetworkBufferPointer buf = packet->toPacket();
    _links[linkNo]->write(buf);

    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "writeRLinkTask", "OK");
}

void WifiP2PChannel::writeWLinkTask(int linkNo) {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "writeWLinkTask", "");

    bool isWritten = false;
    if(_links[WIFIP2P_WRITE_LINK])
    {
        std::shared_ptr<P2PLinkLinkCheckPacket> packet = std::make_shared<P2PLinkLinkCheckPacket>(P2PLinkLinkCheckPacket::WLinkCheck, (uint8_t) linkNo);
        NetworkBufferPointer buf = packet->toPacket();
        if (_links[WIFIP2P_WRITE_LINK]->state() == LinkState::CONNECTED) {
            _links[WIFIP2P_WRITE_LINK]->write(buf);
            isWritten = true;
        }
    }

    if(_links[WIFIP2P_WRITE_V6LINK])
    {
        std::shared_ptr<P2PLinkLinkCheckPacket> packet = std::make_shared<P2PLinkLinkCheckPacket>(P2PLinkLinkCheckPacket::WLinkCheck, (uint8_t) linkNo);
        NetworkBufferPointer buf = packet->toPacket();
        if (_links[WIFIP2P_WRITE_V6LINK]->state() == LinkState::CONNECTED) {
            _links[WIFIP2P_WRITE_V6LINK]->write(buf);
            isWritten = true;
        }
    }
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "writeWLinkTask", "OK");
}

void WifiP2PChannel::selectLinksTask() {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "selectLinksTask", "");

    {
        for (int i = 0; i < readLinkCandidates.size(); i++) {
            readLink = readLinkCandidates[i];
            if (readLink == WIFIP2P_READ_V6LINK)
                break; // v6 prefered
        }
        for (int i = 0; i < writeLinkCandidates.size(); i++) {
            writeLink = writeLinkCandidates[i];
            if (writeLink == WIFIP2P_WRITE_V6LINK)
                break; // v6 prefered
        }

        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                          "Using readLink " + to_string(readLink));
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, __func__,
                          "Using writeLink " + to_string(writeLink));

        if (readLink == -1 || writeLink == -1) {
            // TODO:
        }
    }

    if (_state == P2PACCEPTING) {
        sendHandshakeLNK1();
    }

    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "selectLinksTask", "OK");
}

void WifiP2PChannel::callbackTask() {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "callbackTask", "");

    if (_state == P2PACCEPTING) {
        state(State::CONNECTED);

        if (_p2pAcceptCallback)
            _p2pAcceptCallback(std::dynamic_pointer_cast<P2PChannel>(shared_from_this()));

        sendHandshakeLNK3();
    } else {
        state(State::CONNECTED);

        if (_p2pConnectCallback)
            _p2pConnectCallback(true);
    }

    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "callbackTask", "OK");
}

// link check related
void WifiP2PChannel::startLinkCheck()
{
    stopLinkCheck();

    linkCheckNumRetries = 0;
    readLink = writeLink = -1;
    readLinkCandidates.clear();
    writeLinkCandidates.clear();

    setLinkCheckState(LC_INIT);

    linkCheckTimeoutTask = Anyfi::Timer::schedule([this]{
        onLinkCheckTimeout();
    }, 500);
}

void WifiP2PChannel::stopLinkCheck()
{
    clearLinkCheckTimer();
}

void WifiP2PChannel::clearLinkCheckTimer()
{
    if(linkCheckTimeoutTask != nullptr) {
        Anyfi::Timer::cancel(linkCheckTimeoutTask);
        linkCheckTimeoutTask.reset();
    }
}

void WifiP2PChannel::onLinkCheckTimeout()
{
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "onLinkCheckTimeout", "_linkChechState=" + to_string(_linkCheckState) + ", ticks=" + to_string(_linkCheckStateTicks));

    _linkCheckStateTicks++;

    linkCheckTimeoutTask = Anyfi::Timer::schedule([this]{
        onLinkCheckTimeout();
    }, 500);

    if(_linkCheckStateTicks >= 20) {
        // TODO: no working links
    }

    switch(_linkCheckState) {
        case LC_INIT:
            break;

        case LC_RCHECK: //
        {
            _gotLnk1 = _gotLnk2 = _gotLnk3 = false;
            readLinkCandidates.clear();
            writeLinkCandidates.clear();

            int links[2] = { WIFIP2P_WRITE_LINK, WIFIP2P_WRITE_V6LINK };
            for(int i = 0; i < (sizeof(links) / sizeof(links[0])); i++) {
                int link = links[i];

                if(_links[link] == nullptr || _links[link]->state() != LinkState::CONNECTED)
                    continue;

                writeRLinkTask(link);
            }
            setLinkCheckState(LC_WCHECK);
            break;
        }
        case LC_WCHECK: // waiting wlink
            if(!readLinkCandidates.empty() && !writeLinkCandidates.empty()) {
                setLinkCheckState(LC_LNK12);
                selectLinksTask();
                return;
            }
            if(_linkCheckStateTicks > 5)
                setLinkCheckState(LC_RCHECK); // rcheck again
            break;
        case LC_LNK12: // waiting lnk1 or lnk2
            if(_gotLnk1) {
                sendHandshakeLNK2();
                setLinkCheckState(LC_LNK3);
            }
            else if(_gotLnk2) {
                callbackTask();
                setLinkCheckState(LC_DONE);
            }
            break;
        case LC_LNK3: // waiting lnk2,3
            if(_gotLnk3) {
                callbackTask();
                setLinkCheckState(LC_DONE);
            }
            break;
        case LC_DONE: // done
            stopLinkCheck();
            break;
        default:
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "onLinkCheckTimeout", "Invalid _linkChechState");
            break;
    }
}

void WifiP2PChannel::setLinkCheckState(LinkCheckState state)
{
    _linkCheckState = state;
    _linkCheckStateTicks = 0;
}
