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

#include "Core.h"
#include "L4AnyfiLayer.h"
#include "Log/Frontend/Log.h"
#include "Log/Backend/LogManager.h"
#include "Config.h"

#include "L1LinkLayer/L1LinkLayer.h"

L4AnyfiLayer::L4AnyfiLayer(Anyfi::UUID myUid) : _myUid(myUid) {
    _p2pConnectivityController = std::make_shared<L4::P2PConnectivityController>();
    _bridgeApplication = std::make_shared<L4::AnyfiBridgeApplication>();
}

void L4AnyfiLayer::initialize() {
    _bridgeApplication->attachL4Interface(shared_from_this());

    Anyfi::Log::setPlatformLog([this](int level, const std::string &message) {
        std::string logMessage = message;
        Anyfi::Core::getInstance()->enqueueTask([this, level, logMessage]() {
            _connectivity->logMessage(level, logMessage);
        });
    });
}

void L4AnyfiLayer::terminate() {
    disconnectVPNLink();
}

std::pair<bool, std::vector<Anyfi::NetworkType>> getNetworkPreference(Anyfi::NetworkType trafficType, Anyfi::ConnectionProtocol connectionProtocol) {
    bool main = true;
    std::vector<Anyfi::NetworkType> networkList;
    if (trafficType == Anyfi::NetworkType::INVALID || trafficType == Anyfi::NetworkType::NONE) {
    } else if (trafficType == Anyfi::NetworkType::DEFAULT || trafficType == Anyfi::NetworkType::WIFI || trafficType == Anyfi::NetworkType::MOBILE || trafficType == Anyfi::NetworkType::P2P) {
        networkList.push_back(trafficType);
    } else if (trafficType == Anyfi::NetworkType::DUAL) {
        if (connectionProtocol == Anyfi::ConnectionProtocol::TCP || connectionProtocol == Anyfi::ConnectionProtocol::MeshRUDP) {
            main = false;
            networkList.push_back(Anyfi::NetworkType::MOBILE);
            networkList.push_back(Anyfi::NetworkType::WIFI);
        } else {
            networkList.push_back(Anyfi::NetworkType::WIFI);
        }
    } else if (trafficType == Anyfi::NetworkType::MOBILE_WITH_WIFI_TEST) {
        if (connectionProtocol == Anyfi::ConnectionProtocol::TCP) {
            networkList.push_back(Anyfi::NetworkType::MOBILE);
            networkList.push_back(Anyfi::NetworkType::WIFI);
        } else {
            networkList.push_back(Anyfi::NetworkType::MOBILE);
        }
    }

    return std::make_pair(main, networkList);
}

std::string getIthWord(const std::string &line, int index) {
    size_t pos = 0;
    std::string word;
    int count = 0;
    do {
        std::size_t start = line.find_first_not_of(" ", pos);
        if (start != std::string::npos) {
            std::size_t end = line.find(" ", start + 1);
            if (end == std::string::npos) {
                end = line.length();
            }
            if (count == index) {
                word = line.substr(start, end - start);
                break;
            }
            pos = end + 1;
            count++;
        } else {
            break;
        }
    } while (pos < line.length());
    return word;
}

std::tuple<int, int> getSrcPortAndUid(const std::string &line) {
    std::string localAddr = getIthWord(line, 1);
    if (!localAddr.empty()) {
        size_t portIndex = localAddr.find(":", 0);
        if (portIndex != std::string::npos) {
            std::string portStr = localAddr.substr(portIndex + 1,
                                                   localAddr.length() - portIndex - 1);
            int port = std::stoi(portStr, 0, 16);

            std::string uid = getIthWord(line, 7);
            if (!uid.empty()) {
                auto srcUid = std::stoi(uid);
                if (srcUid > 0) {
                    return std::make_tuple(port, srcUid);
                }
            }
        }
    }

    return std::make_tuple<int, int>(-1, -1);
}

std::unordered_map<int, int> getSrcPortUidMapFromFile(const std::string &filePath) {
    std::unordered_map<int, int> srcPortUidMap;
    if (!filePath.empty()) {
        std::ifstream infile(filePath);
        if (!infile.fail()) {
            std::string line;
            while (std::getline(infile, line)) {
                auto srcPortUidTuple = getSrcPortAndUid(line);
                if (std::get<0>(srcPortUidTuple) > 0) {
                    srcPortUidMap[std::get<0>(srcPortUidTuple)] = std::get<1>(srcPortUidTuple);
                }
            }
            infile.close();
        }
    }

    return srcPortUidMap;
}

int getUidForSrcPortFromFile(int srcPort, const std::string &filePath) {
    // Get Uid
    int srcUid = -1;
    if (!filePath.empty()) {
        std::ifstream infile(filePath);
        if (!infile.fail()) {
            std::string line;
            while (std::getline(infile, line)) {
                auto srcPortUidTuple = getSrcPortAndUid(line);
                // Check port
                if (srcPort == std::get<0>(srcPortUidTuple)) {
                    srcUid = std::get<1>(srcPortUidTuple);
                    break;
                }
            }
            infile.close();
        }
    }

    return srcUid;
}

std::unordered_map<int, int> getSrcPortUidMap() {
    std::unordered_map<int, int> srcPortUidMap;
    std::unordered_map<int, int> tempSrcPortUidMap = getSrcPortUidMapFromFile("/proc/net/tcp");
    srcPortUidMap.insert(tempSrcPortUidMap.begin(), tempSrcPortUidMap.end());
    tempSrcPortUidMap = getSrcPortUidMapFromFile("/proc/net/tcp6");
    srcPortUidMap.insert(tempSrcPortUidMap.begin(), tempSrcPortUidMap.end());
    tempSrcPortUidMap = getSrcPortUidMapFromFile("/proc/net/udp");
    srcPortUidMap.insert(tempSrcPortUidMap.begin(), tempSrcPortUidMap.end());
    tempSrcPortUidMap = getSrcPortUidMapFromFile("/proc/net/udp6");
    srcPortUidMap.insert(tempSrcPortUidMap.begin(), tempSrcPortUidMap.end());

    return srcPortUidMap;
}

int getUidForAddrFromFile(const Anyfi::Address srcAddr, const Anyfi::Address destAddr) {
    int srcUid = -1;
    if (destAddr.connectionProtocol() == Anyfi::ConnectionProtocol::TCP) {
        srcUid = getUidForSrcPortFromFile(srcAddr.port(), "/proc/net/tcp");
        if (srcUid < 0) {
            srcUid = getUidForSrcPortFromFile(srcAddr.port(), "/proc/net/tcp6");
        }
    } else if (destAddr.connectionProtocol() == Anyfi::ConnectionProtocol::UDP) {
        srcUid = getUidForSrcPortFromFile(srcAddr.port(), "/proc/net/udp");
        if (srcUid < 0) {
            srcUid = getUidForSrcPortFromFile(srcAddr.port(), "/proc/net/udp6");
        }
    }

    return srcUid;
}

Anyfi::NetworkType L4AnyfiLayer::getTrafficType(const Anyfi::Address srcAddr, Anyfi::Address destAddr) {
    Anyfi::NetworkType trafficType = Anyfi::NetworkType::INVALID;
    int uid = -1;
    if (_networkMap.size() > 1) {
        uid = getUidForAddrFromFile(srcAddr, destAddr);
        if (uid < 0) {
            uid = getUidForAddr(srcAddr, destAddr);
        }
        trafficType = getTrafficTypeForUid(uid);
    } else if (_networkMap.size() == 1){
        trafficType = getTrafficTypeForUid(uid);
    }

    return trafficType;
}

void L4AnyfiLayer::manage() {
    if (time(NULL) % (60) == 0) {
        Anyfi::LogManager::getInstance()->checkRefresh();
    }
}

//
// ========================================================================================
// IL4AnyfiLayerForL3
//
void L4AnyfiLayer::onSessionConnectFromVPN(unsigned short cbId, Anyfi::Address srcAddr, Anyfi::Address destAddr) {
    auto trafficType = getTrafficType(srcAddr, destAddr);
    auto networkPreference = getNetworkPreference(trafficType, destAddr.connectionProtocol());
    std::vector<Anyfi::Address> gatewayList;
    if (!networkPreference.second.empty()) {
        // Loop through network priority list and use preferred and available network
        for (auto networkType : networkPreference.second) {
            if (networkType == Anyfi::NetworkType::WIFI ||
                networkType == Anyfi::NetworkType::MOBILE ||
                    networkType == Anyfi::NetworkType::DEFAULT) {
                // Check network handle available
//                if (Anyfi::Core::getInstance()->getNetworkHandle(networkType) > 0) {
                    auto gatewayAddr = Anyfi::Address();
                    gatewayAddr.networkType(networkType);
                    gatewayList.push_back(gatewayAddr);
//                }

            } else if (networkType == Anyfi::NetworkType::P2P) {
                // Check gateway available
                auto closestGateway = _L3->getClosestMeshNode(NodeType::Gateway);
                if (!closestGateway.first.isEmpty()) {
                    closestGateway.first.networkType(networkType);
                    gatewayList.push_back(closestGateway.first);
                }
            }
        }
    }

    // Close session if no preference available
    if (gatewayList.empty()) {
        closeSession(cbId, true);
        return;
    }

    _bridgeApplication->onSessionConnectFromVPN(cbId, gatewayList, networkPreference.first, srcAddr, destAddr);
}

void L4AnyfiLayer::onSessionConnectFromMesh(unsigned short cbId, Anyfi::Address destAddr) {
    Anyfi::Address srcAddr;
    auto trafficType = getTrafficType(srcAddr, destAddr);
    auto networkPreference = getNetworkPreference(trafficType, destAddr.connectionProtocol());
    if (!networkPreference.second.empty()) {
        // Loop through network priority list and use preferred and available network
        std::vector<Anyfi::Address> gatewayList;
        for (auto networkType : networkPreference.second) {
            if (networkType == Anyfi::NetworkType::WIFI ||
                networkType == Anyfi::NetworkType::MOBILE ||
                    networkType == Anyfi::NetworkType::DEFAULT) {
                // Check network handle available
//                if (Anyfi::Core::getInstance()->getNetworkHandle(networkType) > 0) {
                    auto gatewayAddr = Anyfi::Address();
                    gatewayAddr.networkType(networkType);
                    gatewayList.push_back(gatewayAddr);
//                }

            } else if (networkType == Anyfi::NetworkType::P2P) {
                throw std::runtime_error("Unsupported network: " + to_string(Anyfi::NetworkType::P2P));
            }
        }

        if (!gatewayList.empty()) {
            _bridgeApplication->onSessionConnectFromMesh(cbId, gatewayList, networkPreference.first);
        }
    }
}

void L4AnyfiLayer::onSessionConnectFromProxy(unsigned short cbId, Anyfi::Address sourceAddr) {
    _bridgeApplication->onSessionConnectFromProxy(cbId, sourceAddr);
}

void L4AnyfiLayer::onReadFromSession(unsigned short cbId, NetworkBufferPointer buffer) {
    _bridgeApplication->onReadFromSession(cbId, buffer);
}

void L4AnyfiLayer::onSessionClose(unsigned short cbId, bool forced) {
    _bridgeApplication->onSessionClose(cbId, forced);
}

void L4AnyfiLayer::onMeshLinkClosed(Anyfi::Address address) {
//    _p2pConnectivityController->disconnectP2P(address);
//
//    std::pair<Anyfi::Address, uint8_t> closestGateway = _L3->getClosestMeshNode(NodeType::Gateway);
//    if (closestGateway.first.isEmpty()) {
//        _stopVPN();
//
//        _p2pConnectivityController->disconnectP2P(Anyfi::Address());
//        _p2pConnectivityController->startAcceptP2P(false);
//        _p2pConnectivityController->discoverP2PGroup(true);
//    } else {
//        if (closestGateway.second > 3) {
//            _p2pConnectivityController->disconnectAcceptedP2P();
//            _p2pConnectivityController->startAcceptP2P(false);
//        }
//    }
}

void L4AnyfiLayer::onMeshLinkAccepted(Anyfi::Address address) {
    _p2pConnectivityController->acceptP2P(address);
}
//
// IL4AnyfiLayerForL3
// ========================================================================================
//


//
// ========================================================================================
// IL4AnyfiLayerForConnectivity
//

void L4AnyfiLayer::onNetworkPreferenceChanged(std::string netPref) {
    Anyfi::LogManager::getInstance()->record()->networkPreferenceChanged(netPref);

    std::unordered_map<int, Anyfi::NetworkType> networkMap;
    auto result = json::parse(netPref);
    for (const auto& item: result.items()) {
        auto uid = std::stoi(item.key());
        auto networkStr = item.value().get<std::string>();
        if (networkStr.compare("none") == 0) {
            networkMap[uid] = Anyfi::NetworkType::NONE;
        } else if (networkStr.compare("default") == 0) {
            networkMap[uid] = Anyfi::NetworkType::DEFAULT;
        } else if (networkStr.compare("wifi") == 0) {
            networkMap[uid] = Anyfi::NetworkType::WIFI;
        } else if (networkStr.compare("mobile") == 0) {
            networkMap[uid] = Anyfi::NetworkType::MOBILE;
        } else if (networkStr.compare("mobile_with_wifi_test") == 0) {
            networkMap[uid] = Anyfi::NetworkType::MOBILE_WITH_WIFI_TEST;
        } else if (networkStr.compare("dual") == 0) {
            networkMap[uid] = Anyfi::NetworkType::DUAL;
        }
    }

    auto prevNetworkMap = _networkMap;
    _networkMap = networkMap;

    // Reset sessions if necessary
    std::unordered_map<int, int> srcPortUidMap = getSrcPortUidMap();
    _bridgeApplication->resetSession(srcPortUidMap);

    // Record data usage
    bool hasWiFi = false;
    auto defaultTrafficEntry = _networkMap.find(-1);
    if (defaultTrafficEntry != _networkMap.end()) {
        auto networkPreference = getNetworkPreference(defaultTrafficEntry->second, Anyfi::ConnectionProtocol::TCP);
        for (auto type : networkPreference.second) {
            if (type == Anyfi::NetworkType::WIFI) {
                hasWiFi = true;
                break;
            }
        }
    }
    if (_hasWiFi != hasWiFi) {
        _hasWiFi = hasWiFi;
        Anyfi::LogManager::getInstance()->recordNetworkTraffic();
    }
}

void L4AnyfiLayer::connectP2PLink(Anyfi::Address addr) {}

void L4AnyfiLayer::disconnectP2PLink(Anyfi::Address addr) {}

void L4AnyfiLayer::connectVPNLink(int fd) {
    Anyfi::Config::lock_type guard(_vpnMutex);

    bool success = false;
    if (fd < 0) {
        Anyfi::Log::error(__func__, "Start VPN Failed");
    } else if (fd == 0) {
        _L3->openExternalVPN();
        success = true;
    } else {
        _L3->openInternalVPN(fd);
        success = true;
    }

    Anyfi::LogManager::getInstance()->record()->connectVpnLinkResult(fd, success);
    _connectivity->onVPNLinkConnected(success);
}

void L4AnyfiLayer::disconnectVPNLink() {
    Anyfi::Config::lock_type guard(_vpnMutex);
    _L3->closeVPN();
    Anyfi::LogManager::getInstance()->record()->disconnectVpnLinkResult(true);
}

//
// IL4AnyfiLayerForConnectivity
// ========================================================================================
//


//
// ========================================================================================
// IL4AnyfiLayerForApplication
//
void L4AnyfiLayer::connectSession(Anyfi::Address address,
                                  std::function<void(unsigned short)> callback) {
    _L3->connectSession(address, callback);
}

void L4AnyfiLayer::closeSession(unsigned short cbId, bool force) {
    _L3->closeSession(cbId, force);
}

void L4AnyfiLayer::write(unsigned short cbId, NetworkBufferPointer buffer) {
    _L3->write(cbId, buffer);
}

int L4AnyfiLayer::getUidForAddr(Anyfi::Address srcAddr, Anyfi::Address destAddr) {
    return _connectivity->getUidForAddr(destAddr.connectionProtocol(), srcAddr.addr(), srcAddr.port(), destAddr.addr(), destAddr.port());;
}

Anyfi::NetworkType L4AnyfiLayer::getTrafficTypeForUid(int uid) {
    Anyfi::NetworkType trafficType = Anyfi::NetworkType::INVALID;

    if (uid > 0) {
        auto trafficEntry = _networkMap.find(uid);
        if (trafficEntry != _networkMap.end()) {
            trafficType = trafficEntry->second;
        }
    }

    // Use default traffic if not defined yet
    if (trafficType == Anyfi::NetworkType::INVALID) {
        auto defaultTrafficEntry = _networkMap.find(-1);
        if (defaultTrafficEntry != _networkMap.end()) {
            trafficType = defaultTrafficEntry->second;
        }
    }

    return trafficType;
}
//
// IL4AnyfiLayerForApplication
// ========================================================================================
//


//
// ========================================================================================
// L4AnyfiLayer private
//

void L4AnyfiLayer::dump() {
    _bridgeApplication->dump();
}