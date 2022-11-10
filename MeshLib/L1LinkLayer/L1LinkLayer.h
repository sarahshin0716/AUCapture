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

#ifndef ANYFIMESH_CORE_L1LINKLAYER_H
#define ANYFIMESH_CORE_L1LINKLAYER_H


// Layer interfaces
#include "IL1LinkLayer.h"


#include "../L2NetworkLayer/IL2NetworkLayer.h"
#include "../IOTunnel/IOTunnel.h"
#include "../Common/json.hpp"
#include "../Config.h"
#include "../Common/Timer.h"

#include "Channel/Channel.h"
#include "Channel/P2PChannel.h"
#include "Channel/ProxyChannel.h"
#include "Channel/VPN/VpnChannel.h"

#include <set>
#include <utility>

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif


using namespace nlohmann;

// Forward declarations : Link.h
class Link;
// Forward declarations : AcceptServerLink.h
class AcceptServerLink;
// Forward declarations : WifiP2PLink.h
class WifiP2PLink;

// Forward declarations : IOEngine.h
class IOEngine;

// Forward declarations : this
class L1LinkLayer;
class ConcreteL1LinkLayer;


/**
 * AnyfiMesh Layer 1. LinkLayer
 * This layer manages network links and performs I/O operations.
 *
 * To learn more about L1LinkLayer,
 * goto : https://bitbucket.org/anyfi/anyfimesh-core/wiki/L1LinkLayer%20Architecture
 */
class L1LinkLayer : public IL1LinkLayerForL2, public IL1LinkLayerForIOTunnel {
public:
    /**
     * These methods can only be invoked at AnyfiMesh class.
     */
    static std::shared_ptr<L1LinkLayer> init();
    /**
     * Terminate L1 operation and delete L1 instance.
     */
    static void terminate();
    L1LinkLayer(const L1LinkLayer &other) = delete;
    L1LinkLayer &operator=(const L1LinkLayer &other) = delete;
    void manage();
private:
    friend class Link;
    friend class ConcreteL1LinkLayer;
    /**
     * Only Link class can access to the instance exceptionally.
     */
    static std::shared_ptr<L1LinkLayer> getInstance();
    L1LinkLayer();
    ~L1LinkLayer();
    void destructorImpl();
    static std::shared_ptr<ConcreteL1LinkLayer> _instance;

public:
    void attachL2Interface(std::shared_ptr<IL2NetworkLayerForL1> l2Interface) { _L2 = std::move(l2Interface); }
    void attachIOTunnelInterface(std::shared_ptr<IOTunnel> ioTunnelInterface) { _ioTunnel = std::move(ioTunnelInterface); }

    /**
     * Override interfaces : IL1LinkLayerForIOTunnel
     */
    void onExternalVpnLinkStateChanged(BaseChannel::State state) override;

    /**
     * Override interfaces : IL1LinkLayerForL2
     */
    ChannelInfo openInternalVpnChannel(int fd) override;
    ChannelInfo openExternalVpnChannel() override;
    void connectChannel(Anyfi::Address address, const std::function<void(ChannelInfo linkInfo)> &callback) override;
    ChannelInfo openChannel(Anyfi::Address linkAddr) override;
    void closeChannel(unsigned short channelId) override;
    void writeToChannel(unsigned short channelId, NetworkBufferPointer buffer) override;
private:
    // Layer interfaces
    std::shared_ptr<IL2NetworkLayerForL1> _L2;
    std::shared_ptr<IOTunnel> _ioTunnel;

    // IOEngine instance & interface
    std::shared_ptr<IOEngine> _ioEngine;
    std::shared_ptr<Selector> _selector;
    void onRead(std::shared_ptr<BaseChannel> channel, const std::shared_ptr<Link> &link, NetworkBufferPointer buffer);
    void onLinkAccepted(std::shared_ptr<BaseChannel> acceptFrom, std::shared_ptr<Link> acceptedChannel);
    void onP2ChannelAccepted(std::shared_ptr<P2PChannel> p2pChannel);
    void onClose(std::shared_ptr<BaseChannel> channel);

    // Link-relative entities
    Anyfi::Config::mutex_type _channelMutex;
    std::unordered_map<unsigned short, std::shared_ptr<BaseChannel>> _channelMap;
    std::shared_ptr<VpnChannel> _vpnChannel;
    void _addNewChannel(std::shared_ptr<BaseChannel> channel);
    void _removeChannel(std::shared_ptr<BaseChannel> channel);

    unsigned short _getUnassignedChannelID();

    std::shared_ptr<BaseChannel> _findChannel(unsigned short channelId);
    inline std::shared_ptr<BaseChannel> _findChannelWithoutLock(unsigned short channelId);
    ChannelInfo _getChannelInfo(unsigned short link_id);
    ChannelInfo _getChannelInfo(std::shared_ptr<BaseChannel> channel);

    // Link Acceptor relative entities
    Anyfi::Config::mutex_type _acceptorMutex;
    std::set<std::shared_ptr<SimpleChannel>> _acceptors;




#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(L1LinkLayer, AddNewChannel);
    FRIEND_TEST(L1LinkLayer, RemoveLink);
    FRIEND_TEST(L1LinkLayer, MultiThreadAddNewLink);
    FRIEND_TEST(L1LinkLayer, MultiThreadRemoveLink);
    FRIEND_TEST(L1LinkLayer, GetUnassignedLinkID);
    FRIEND_TEST(L1LinkLayer, FindLink);
    FRIEND_TEST(L1LinkLayer, GetLinkInfo);
    FRIEND_TEST(L1LinkLayer, AcceptWifiP2PLink);
    FRIEND_TEST(L1LinkLayer, connect2L1LinkLayer);
#endif
public:
    /**
     * Type of Network Performance Variable.
     */
    enum class NetworkPerformanceType {
        ID,
        IP_ADDR,
        PROTOCOL,
        RTT,
        RTT_VAR,
        READ_BYTES,
        WRITTEN_BYTES,
    };

    class NetworkPerformance {
    public:
        std::unordered_map<unsigned short, std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher>> socketPerformanceMap;
        std::unordered_map<unsigned short, std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher>> congestedSocketMap;

        std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> getSocketPerformance(unsigned short id) {
            std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> result;
            auto entry = socketPerformanceMap.find(id);
            if (entry != socketPerformanceMap.end()) {
                result = (*entry).second;
            }
            return result;
        }

        void addSocketPerformance(unsigned short id, std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> socketPerformance){
            socketPerformanceMap[id] = socketPerformance;
        }

        void addCongestedSocket(unsigned short id, std::unordered_map<NetworkPerformanceType, __uint64_t, EnumHasher> congestedSocket) {
            congestedSocketMap[id] = congestedSocket;
        }

        void clear() {
            socketPerformanceMap.clear();
            congestedSocketMap.clear();
        }

        json toJson() {
            json perf;
            if(socketPerformanceMap.size() > 0) {
                json socketPerfList = json::array();
                for (auto& perfEntry : socketPerformanceMap) {
                    auto perfMap = perfEntry.second;
                    json socketPerf;
                    for (auto& pair : perfMap) {
                        socketPerf[netPerfTypeToString(pair.first)] = pair.second;
                    }
                    socketPerfList.push_back(socketPerf);
                }
                perf["sockets"] = socketPerfList;
            }
            if (congestedSocketMap.size() > 0) {
                json congestedSocketList = json::array();
                for (auto& congestedEntry : congestedSocketMap) {
                    auto congestedMap = congestedEntry.second;
                    json congestedSocket;
                    for (auto& pair : congestedMap) {
                        congestedSocket[netPerfTypeToString(pair.first)] = pair.second;
                    }
                    congestedSocketList.push_back(congestedSocket);
                }
                perf["congested_sockets"] = congestedSocketList;
            }
            return perf;
        }
    private:
        std::string netPerfTypeToString(NetworkPerformanceType type) {
            switch (type) {
                case NetworkPerformanceType::ID:
                    return "id";
                case NetworkPerformanceType::IP_ADDR:
                    return "ip_addr";
                case NetworkPerformanceType::PROTOCOL:
                    return "protocol";
                case NetworkPerformanceType::RTT:
                    return "rtt";
                case NetworkPerformanceType::RTT_VAR:
                    return "rtt_var";
                case NetworkPerformanceType::READ_BYTES:
                    return "read_bytes";
                case NetworkPerformanceType::WRITTEN_BYTES:
                    return "written_bytes";
            }
            return "";
        }
    };

    void saveTCPPerformance(std::shared_ptr<ProxyChannel>& proxyChannel);
    void saveUDPPerformance(std::shared_ptr<ProxyChannel>& proxyChannel);
    std::string getSocketPerformance();
    std::shared_ptr<NetworkPerformance> _wifiPerformance, _mobilePerformance;

    void dump();
};


#endif //ANYFIMESH_CORE_L1LINKLAYER_H
