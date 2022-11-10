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

#include "L1LinkLayer.h"

#include <utility>
#include <functional>
#include <linux/tcp.h>

#include "Common/Timer.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "Log/Frontend/Log.h"
#include "Log/Backend/LogManager.h"
#include "Core.h"
#include "L1LinkLayer/IOEngine/IOEngine.h"
#include "L1LinkLayer/Channel/ProxyChannel.h"
#include "L1LinkLayer/Channel/VPN/VpnChannel.h"
#include "L1LinkLayer/Channel/WifiP2PChannel.h"
#include "L1LinkLayer/Channel/WifiP2PAcceptChannel.h"
#include "L1LinkLayer/Channel/WifiP2PChannel.h"
#include "L1LinkLayer/Channel/WifiP2PAcceptChannel.h"
#include "L1LinkLayer/Link/Link.h"
#include "L1LinkLayer/Link/SocketLink.h"
#include "L1LinkLayer/Link/BluetoothP2PLink.h"
#include "L1LinkLayer/IOEngine/IOEngine.h"

class ConcreteL1LinkLayer : public L1LinkLayer {
public:
    ~ConcreteL1LinkLayer() override = default;
};

std::shared_ptr<ConcreteL1LinkLayer> L1LinkLayer::_instance = nullptr;


std::shared_ptr<L1LinkLayer> L1LinkLayer::init() {
    if (_instance == nullptr) {
        _instance = std::make_shared<ConcreteL1LinkLayer>();
    }
    return _instance;
}

void L1LinkLayer::terminate() {
    if (_instance != nullptr)
        _instance->destructorImpl();
    _instance = nullptr;
}

std::shared_ptr<L1LinkLayer> L1LinkLayer::getInstance() {
    return _instance;
}

L1LinkLayer::L1LinkLayer() {
    _selector = Selector::open();

    _ioEngine = IOEngine::start(_selector);
    std::srand((unsigned int) std::time(nullptr));

    _wifiPerformance = std::make_shared<NetworkPerformance>();
    _mobilePerformance = std::make_shared<NetworkPerformance>();
}

L1LinkLayer::~L1LinkLayer() {
    destructorImpl();
}

void L1LinkLayer::destructorImpl() {
    if (_selector != nullptr) {
        _selector->close();
        _selector = nullptr;
    }

    if (_ioEngine != nullptr) {
        _ioEngine->shutdown();
    }

    {
        Anyfi::Config::lock_type guard(_channelMutex);
        for (auto &channelEntry: _channelMap) {
            auto channel = channelEntry.second;
            channel->close();
        }
    }

    NetworkBufferPool::clear();
}

void L1LinkLayer::manage() {
    // Check proxy socket connect timeout
    std::vector<std::shared_ptr<Link>> linkList;
    {
        Anyfi::Config::lock_type guard(_channelMutex);
        for (auto &channelEntry: _channelMap) {
            auto channel = channelEntry.second;
            if(!channel->addr().addr().empty() && channel->addr().connectionType() == Anyfi::ConnectionType::Proxy) {
                auto proxyChannel = std::dynamic_pointer_cast<ProxyChannel>(channel);
                if (channel->addr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
                    linkList.push_back(proxyChannel->link());
                }
            }
        }
    }
    for (auto link : linkList) {
        link->checkTimeout();
    }
}

//
// ========================================================================================
// Override interfaces : IL1LinkLayerForIOTunnel
//
void L1LinkLayer::onExternalVpnLinkStateChanged(BaseChannel::State state) {
    if (_vpnChannel == nullptr) {
        return;
    }

    if (state == BaseChannel::State::CONNECTED) {
        _vpnChannel->state(BaseChannel::State::CONNECTED);
    } else if (state == BaseChannel::State::CLOSED) {
        _L2->onClosed(_vpnChannel->id());

        _vpnChannel->close();
        _vpnChannel = nullptr;
    } else {
        throw std::invalid_argument("Invalid External VPN Link State");
    }
}
//
// Override interfaces : IL1LinkLayerForIOTunnel
// ========================================================================================
//


//
// ========================================================================================
// Override interfaces : IL1LinkLayerForL2
//
ChannelInfo L1LinkLayer::openInternalVpnChannel(int fd) {
    if (_vpnChannel == nullptr) {
        _vpnChannel = std::make_shared<VpnChannel>(_getUnassignedChannelID());
        _vpnChannel->setReadCallback(std::bind(&L1LinkLayer::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _vpnChannel->setCloseCallback(std::bind(&L1LinkLayer::onClose, this, std::placeholders::_1));
        _addNewChannel(_vpnChannel);
    }

    // Opens a vpn link
    Anyfi::Address address;
    address.addr("127.0.0.1");
    address.port(static_cast<uint16_t>(fd));
    address.connectionType(Anyfi::ConnectionType::VPN);
    _vpnChannel->open(address);

    return _getChannelInfo(_vpnChannel);
}

ChannelInfo L1LinkLayer::openExternalVpnChannel() {
    if (_vpnChannel == nullptr) {
        _vpnChannel = std::make_shared<VpnChannel>(_getUnassignedChannelID(), _ioTunnel);
        _vpnChannel->setReadCallback(std::bind(&L1LinkLayer::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _vpnChannel->setCloseCallback(std::bind(&L1LinkLayer::onClose, this, std::placeholders::_1));
        _addNewChannel(_vpnChannel);
    }

    Anyfi::Address address = Anyfi::Address();
    address.connectionType(Anyfi::ConnectionType::VPN);
    _vpnChannel->open(address);

    return _getChannelInfo(_vpnChannel);
}

void L1LinkLayer::connectChannel(Anyfi::Address address, const std::function<void(ChannelInfo linkInfo)> &callback) {
    switch (address.connectionType()) {
        case Anyfi::ConnectionType::Proxy: {
            auto proxyChannel = std::make_shared<ProxyChannel>(_getUnassignedChannelID(), _selector, address);
            proxyChannel->setReadCallback(std::bind(&L1LinkLayer::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            proxyChannel->setCloseCallback(std::bind(&L1LinkLayer::onClose, this, std::placeholders::_1));
            _addNewChannel(proxyChannel);
            proxyChannel->connect(address, [this, callback, proxyChannel](bool success){
                if (!success) {
                    _removeChannel(proxyChannel);
                }

                callback(success ? _getChannelInfo(proxyChannel) : ChannelInfo());
            });
            break;
        }
        case Anyfi::ConnectionType::WifiP2P: {
            auto wifip2pChannel = std::make_shared<WifiP2PChannel>(_getUnassignedChannelID(), _selector, address, false);
            wifip2pChannel->setReadCallback(std::bind(&L1LinkLayer::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            wifip2pChannel->setCloseCallback(std::bind(&L1LinkLayer::onClose, this, std::placeholders::_1));
            _addNewChannel(wifip2pChannel);
            wifip2pChannel->connect(address, [this, callback, wifip2pChannel](bool success) {
                if (success) {
                    callback(_getChannelInfo(wifip2pChannel));
                }else {
                    _removeChannel(wifip2pChannel);
                    callback(ChannelInfo());
                }
            });
            break;
        }
        case Anyfi::ConnectionType::BluetoothP2P:
            break;
        default:
            throw std::invalid_argument("Unsupported ConnectionType in L1LinkLayer");
    }
}

ChannelInfo L1LinkLayer::openChannel(Anyfi::Address linkAddr) {
    switch (linkAddr.connectionType()) {
        case Anyfi::ConnectionType::WifiP2P: {
            auto wifiP2PAcceptChannel = std::make_shared<WifiP2PAcceptChannel>(_getUnassignedChannelID(), _selector);
            wifiP2PAcceptChannel->setAcceptCallback(std::bind(&L1LinkLayer::onLinkAccepted, this, std::placeholders::_1, std::placeholders::_2));
            wifiP2PAcceptChannel->setCloseCallback(std::bind(&L1LinkLayer::onClose, this, std::placeholders::_1));
            wifiP2PAcceptChannel->open(linkAddr);
            {
                Anyfi::Config::lock_type guard(_acceptorMutex);
                _acceptors.insert(wifiP2PAcceptChannel);
            }
            return _getChannelInfo(wifiP2PAcceptChannel);
        }
        case Anyfi::ConnectionType::BluetoothP2P: {
            return ChannelInfo();
        }
        case Anyfi::ConnectionType::Proxy: {
            auto proxyChannel = std::make_shared<ProxyChannel>(_getUnassignedChannelID(), _selector, linkAddr);
            proxyChannel->setReadCallback(std::bind(&L1LinkLayer::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            proxyChannel->setCloseCallback(std::bind(&L1LinkLayer::onClose, this, std::placeholders::_1));
            if (proxyChannel->open(std::move(linkAddr))) {
                _addNewChannel(proxyChannel);
                return _getChannelInfo(proxyChannel);
            }
            return ChannelInfo();
        }
        default:
            throw std::invalid_argument("Unsupported ConnectionType in L1LinkLayer");
    }
}

void L1LinkLayer::closeChannel(unsigned short channelId) {
    auto channel = _findChannel(channelId);
    if (channel == nullptr) {
        // already removed.
        return;
    }

    _removeChannel(channel);

    if (instanceof<WifiP2PAcceptChannel>(channel.get())) {
        Anyfi::Config::lock_type guard(_acceptorMutex);
        _acceptors.erase(std::dynamic_pointer_cast<SimpleChannel>(channel));
    } else if (instanceof<ProxyChannel>(channel.get())) {
        // Save performance
        auto proxyChannel = std::dynamic_pointer_cast<ProxyChannel>(channel);
        if (channel->addr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            saveTCPPerformance(proxyChannel);
        } else if (channel->addr().connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
            saveUDPPerformance(proxyChannel);
        }
    }

    channel->close();

}

void L1LinkLayer::writeToChannel(unsigned short channelId, NetworkBufferPointer buffer) {
    auto channel = _findChannel(channelId);
    if (channel == nullptr) {
        _L2->onClosed(channelId);
        return;
    }
    if (channel->state() == BaseChannel::State::CONNECTED ||
        channel->state() == BaseChannel::State::CONNECTING) {
        channel->write(buffer);
    } else {
        // channel->state() == BaseChannel::State ::CLOSED
        onClose(channel);
    }
}
//
// Override interfaces : IL1LinkLayerForL2
// ========================================================================================
//


//
// ========================================================================================
// L1LinkLayer Private Methods
//
void L1LinkLayer::onRead(std::shared_ptr<BaseChannel> channel, const std::shared_ptr<Link> &link, NetworkBufferPointer buffer) {
    if (_L2 == nullptr)
        return;

    if (channel->state() == BaseChannel::State::CONNECTED) {
        _L2->onReadFromChannel(channel->id(), link, std::move(buffer));
    }
}

void L1LinkLayer::onLinkAccepted(std::shared_ptr<BaseChannel> acceptFrom, std::shared_ptr<Link> acceptedLink) {
    // P2P Socket Accepted
    if (instanceof<WifiP2PAcceptChannel>(acceptFrom.get())) {
        auto p2pChannel = std::make_shared<WifiP2PChannel>(_getUnassignedChannelID(), acceptedLink, acceptedLink->addr(), true);
        p2pChannel->setReadCallback(std::bind(&L1LinkLayer::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        p2pChannel->setP2PAcceptCallback(std::bind(&L1LinkLayer::onP2ChannelAccepted, this, std::placeholders::_1));
        p2pChannel->setCloseCallback(std::bind(&L1LinkLayer::onClose, this, std::placeholders::_1));
        acceptedLink->channel(p2pChannel);
        _addNewChannel(p2pChannel);
        p2pChannel->doHandshake();
    }
}

void L1LinkLayer::onP2ChannelAccepted(std::shared_ptr<P2PChannel> p2pChannel) {
    if (p2pChannel->state() == BaseChannel::State::CONNECTED) {
        _L2->onChannelAccepted(_getChannelInfo(p2pChannel));
    }
}

void L1LinkLayer::onClose(std::shared_ptr<BaseChannel> channel) {
    auto prevChannel = _findChannel(channel->id());
    if (prevChannel == nullptr) {
        // already removed.
        return;
    }

    _removeChannel(channel);

    if (instanceof<WifiP2PAcceptChannel>(channel.get())) {
        Anyfi::Config::lock_type guard(_acceptorMutex);
        _acceptors.erase(std::dynamic_pointer_cast<SimpleChannel>(channel));
    } else if (instanceof<ProxyChannel>(channel.get())) {
        // Save performance
        auto proxyChannel = std::dynamic_pointer_cast<ProxyChannel>(channel);
        if (channel->addr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
            saveTCPPerformance(proxyChannel);
        } else if (channel->addr().connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
            saveUDPPerformance(proxyChannel);
        }
    }

    _L2->onClosed(channel->id());
}

unsigned short L1LinkLayer::_getUnassignedChannelID() {
    Anyfi::Config::lock_type guard(_channelMutex);

    unsigned short id = 0;
    while (id == 0 || _findChannelWithoutLock(id) != nullptr) {
        id = static_cast<unsigned short>(std::rand() % USHRT_MAX + 1);
    }

    return id;
}

void L1LinkLayer::_addNewChannel(std::shared_ptr<BaseChannel> channel) {
    Anyfi::Config::lock_type guard(_channelMutex);

    _channelMap[channel->id()] = channel;
}

void L1LinkLayer::_removeChannel(std::shared_ptr<BaseChannel> channel) {
    Anyfi::Config::lock_type guard(_channelMutex);

    _channelMap.erase(channel->id());

    if (channel == _vpnChannel)
        _vpnChannel = nullptr;
}

std::shared_ptr<BaseChannel> L1LinkLayer::_findChannel(unsigned short channelId) {
    Anyfi::Config::lock_type guard(_channelMutex);
    return _findChannelWithoutLock(channelId);
}

inline std::shared_ptr<BaseChannel> L1LinkLayer::_findChannelWithoutLock(unsigned short channelId) {
    auto channelIter = _channelMap.find(channelId);
    if (channelIter != _channelMap.end()) {
        return channelIter->second;
    }
    return nullptr;
}

ChannelInfo L1LinkLayer::_getChannelInfo(unsigned short link_id) {
    return _getChannelInfo(_findChannel(link_id));
}

ChannelInfo L1LinkLayer::_getChannelInfo(std::shared_ptr<BaseChannel> channel) {
    if (channel == nullptr)
        return ChannelInfo();

    return ChannelInfo(channel->id(), channel->addr());
}

//
// L1LinkLayer Private Methods
// ========================================================================================
//


std::string L1LinkLayer::getSocketPerformance() {
    Anyfi::Config::lock_type guard(_channelMutex);

    // Save socket performance
    for (auto &channelEntry: _channelMap) {
        // Measure Proxy Performance
        auto channel = channelEntry.second;
        if(!channel->addr().addr().empty() && channel->addr().connectionType() == Anyfi::ConnectionType::Proxy) {
            auto proxyChannel = std::dynamic_pointer_cast<ProxyChannel>(channel);
            if (channel->addr().connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
                saveTCPPerformance(proxyChannel);
            } else if (channel->addr().connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
                saveUDPPerformance(proxyChannel);
            }
        }
    }

    // Convert to json string
    json result;
    json wifiPerf = _wifiPerformance->toJson();
    result["wifi"] = wifiPerf;
    json mobilePerf = _mobilePerformance->toJson();
    result["mobile"] = mobilePerf;

    // Clear performance
    _wifiPerformance->clear();
    _mobilePerformance->clear();

    return result.dump();
}

void L1LinkLayer::saveTCPPerformance(std::shared_ptr<ProxyChannel>& proxyChannel) {
    if (proxyChannel == nullptr || proxyChannel->link() == nullptr) {
        return;
    }

    // Get most recent TCP info
    proxyChannel->updateTCPInfo();

    // Get relevant performance
    auto performance = _wifiPerformance;
    if (proxyChannel->addr().networkType() == Anyfi::NetworkType::MOBILE) {
        performance = _mobilePerformance;
    } else if (proxyChannel->addr().networkType() != Anyfi::NetworkType::WIFI) {
        if (Anyfi::Core::getInstance()->getNetworkHandle(Anyfi::NetworkType::MOBILE) > 0) {
            performance = _mobilePerformance;
        }
    }

    int recentReadBytes = proxyChannel->link()->readBytes - proxyChannel->link()->prevReadBytes;
    int recentWrittenBytes = proxyChannel->link()->writtenBytes - proxyChannel->link()->prevWrittenBytes;

    // Save performance of used sockets
    bool hasPerformance = proxyChannel->getPerfChanged() || recentReadBytes > 0;
    if (hasPerformance || recentWrittenBytes > 0) {
        std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> performanceMap = performance->getSocketPerformance(proxyChannel->id());
        performanceMap[NetworkPerformanceType::ID] = proxyChannel->id();
        performanceMap[NetworkPerformanceType::IP_ADDR] = proxyChannel->getIpAddr();
        performanceMap[NetworkPerformanceType::PROTOCOL] = Anyfi::ConnectionProtocol::TCP;
        auto entry = performanceMap.find(NetworkPerformanceType::READ_BYTES);
        if (entry != performanceMap.end()) {
            recentReadBytes += (*entry).second;
        }
        performanceMap[NetworkPerformanceType::READ_BYTES] = recentReadBytes;
        entry = performanceMap.find(NetworkPerformanceType::WRITTEN_BYTES);
        if (entry != performanceMap.end()) {
            recentWrittenBytes += (*entry).second;
        }
        performanceMap[NetworkPerformanceType::WRITTEN_BYTES] = recentWrittenBytes;

        if (hasPerformance && proxyChannel->getRtt() > 0) {
            performanceMap[NetworkPerformanceType::RTT] = proxyChannel->getRtt();
            performanceMap[NetworkPerformanceType::RTT_VAR] = proxyChannel->getRttVar();
        }

        performance->addSocketPerformance(proxyChannel->id(), performanceMap);

        // Reset variables
        proxyChannel->setPerfChanged(false);
        proxyChannel->link()->prevReadBytes = proxyChannel->link()->readBytes;
        proxyChannel->link()->prevWrittenBytes = proxyChannel->link()->writtenBytes;
    }

    // Save congested performance of used socket
    if (!hasPerformance) {
        bool isCongested = false;
        unsigned long rtt = 0;
        if (proxyChannel->state() == BaseChannel::CONNECTING && proxyChannel->link() != nullptr) {
            rtt = (unsigned long) (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - proxyChannel->link()->_sockConnectStart).count());
            isCongested = rtt > PROXY_SOCKET_MIN_ELAPSE_MICRO_SEC * Anyfi::Core::getInstance()->getSensitivity() && rtt < PROXY_SOCKET_MIN_ELAPSE_MICRO_SEC * Anyfi::Core::getInstance()->getSensitivity() + 1000000;
        } else if (proxyChannel->hasLastWriteTime()) {
            rtt = (unsigned long) (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - proxyChannel->getLastWriteTime()).count());
            isCongested = rtt > PROXY_SOCKET_MIN_ELAPSE_MICRO_SEC * Anyfi::Core::getInstance()->getSensitivity();
        }

        if (isCongested) {
            // Track Wi-Fi only
            if (proxyChannel->addr().networkType() == Anyfi::NetworkType::WIFI) {
                std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> performanceMap;
                performanceMap[NetworkPerformanceType::ID] = proxyChannel->id();
                performanceMap[NetworkPerformanceType::IP_ADDR] = proxyChannel->getIpAddr();
                performanceMap[NetworkPerformanceType::PROTOCOL] = Anyfi::ConnectionProtocol::TCP;
                performanceMap[NetworkPerformanceType::RTT] = rtt;
                performance->addCongestedSocket(proxyChannel->id(), performanceMap);
            }

            // Reset variables
            proxyChannel->clearLastWriteTime();
        }
    }
}

void L1LinkLayer::saveUDPPerformance(std::shared_ptr<ProxyChannel>& proxyChannel) {
    if (proxyChannel == nullptr || proxyChannel->link() == nullptr) {
        return;
    }

    // Get relevant performance
    auto performance = _wifiPerformance;
    if (proxyChannel->addr().networkType() == Anyfi::NetworkType::MOBILE) {
        performance = _mobilePerformance;
    } else if (proxyChannel->addr().networkType() != Anyfi::NetworkType::WIFI) {
        if (Anyfi::Core::getInstance()->getNetworkHandle(Anyfi::NetworkType::MOBILE) > 0) {
            performance = _mobilePerformance;
        }
    }

    int recentReadBytes = proxyChannel->link()->readBytes - proxyChannel->link()->prevReadBytes;
    int recentWrittenBytes = proxyChannel->link()->writtenBytes - proxyChannel->link()->prevWrittenBytes;

    // Save performance of used sockets
    bool hasPerformance = proxyChannel->getPerfChanged() || recentReadBytes > 0;
    if (hasPerformance || recentWrittenBytes > 0) {
        std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> performanceMap = performance->getSocketPerformance(proxyChannel->id());
        performanceMap[NetworkPerformanceType::ID] = proxyChannel->id();
        performanceMap[NetworkPerformanceType::IP_ADDR] = proxyChannel->getIpAddr();
        performanceMap[NetworkPerformanceType::PROTOCOL] = Anyfi::ConnectionProtocol::UDP;
        auto entry = performanceMap.find(NetworkPerformanceType::READ_BYTES);
        if (entry != performanceMap.end()) {
            recentReadBytes += (*entry).second;
        }
        performanceMap[NetworkPerformanceType::READ_BYTES] = recentReadBytes;
        entry = performanceMap.find(NetworkPerformanceType::WRITTEN_BYTES);
        if (entry != performanceMap.end()) {
            recentWrittenBytes += (*entry).second;
        }
        performanceMap[NetworkPerformanceType::WRITTEN_BYTES] = recentWrittenBytes;

        performance->addSocketPerformance(proxyChannel->id(), performanceMap);

        // Reset variables
        proxyChannel->setPerfChanged(false);
        proxyChannel->link()->prevReadBytes = proxyChannel->link()->readBytes;
        proxyChannel->link()->prevWrittenBytes = proxyChannel->link()->writtenBytes;
    }

    // Save congested performance of used socket
    if (!hasPerformance) {
        bool isCongested = false;
        unsigned long rtt = 0;
        if (proxyChannel->hasLastWriteTime()) {
            rtt = (unsigned long) (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - proxyChannel->getLastWriteTime()).count());
            isCongested = rtt > PROXY_SOCKET_MIN_ELAPSE_MICRO_SEC * Anyfi::Core::getInstance()->getSensitivity();
        }

        if (isCongested) {
            // Track Wi-Fi only
            if (proxyChannel->addr().networkType() == Anyfi::NetworkType::WIFI) {
                std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> performanceMap;
                performanceMap[NetworkPerformanceType::ID] = proxyChannel->id();
                performanceMap[NetworkPerformanceType::IP_ADDR] = proxyChannel->getIpAddr();
                performanceMap[NetworkPerformanceType::PROTOCOL] = Anyfi::ConnectionProtocol::UDP;
                performanceMap[NetworkPerformanceType::RTT] = rtt;
                performance->addCongestedSocket(proxyChannel->id(), performanceMap);
            }

            // Reset variables
            proxyChannel->clearLastWriteTime();
        }
    }
}

void L1LinkLayer::dump() {
#ifdef NDEBUG
#else
    size_t numChannels = _channelMap.size();

    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "L1LinkLayer::dump", "dumpChannels numChannels=" + to_string(numChannels));

    for (auto &channelEntry: _channelMap) {
        auto channel = channelEntry.second;
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1, "L1LinkLayer::dump dumpChannels ----", channel->toString());
    }
#endif
}
