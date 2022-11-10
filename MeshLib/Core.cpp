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

#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "Common/Timer.h"

#include "L1LinkLayer/L1LinkLayer.h"
#include "L2NetworkLayer/L2NetworkLayer.h"
#include "L3TransportLayer/L3TransportLayer.h"
#include "L4AnyfiLayer/L4AnyfiLayer.h"

static Anyfi::Core *_instance = nullptr;

Anyfi::Core *Anyfi::Core::getInstance() {
    if (!_instance) {
        _instance = new Anyfi::Core();
    }
    return _instance;
}

void Anyfi::Core::init(UUID myUid, std::string myDeviceInfo,
                       std::shared_ptr<IConnectivityForL4> connectivity,
                       std::shared_ptr<IOTunnel> ioTunnel) {
    _networkMode = 0;
    _networkHandle = 0;
    _mobileNetworkHandle = 0;
    _myUid = myUid;
    _connectivity = connectivity;
    _ioTunnel = ioTunnel;

    auto result = json::parse(myDeviceInfo);
    _myDeviceName = result["name"];
    _sensitivity = result["sensitivity"];
}


std::shared_ptr<IL4AnyfiLayerForConnectivity> Anyfi::Core::l4Interface() {
    return std::move(_L4);
}

void Anyfi::Core::start() {
    if (_L1 || _L2 || _L3 || _L4) {
        terminate();
    }
    Anyfi::CoreThread::create("core", 1);

    _L1 = L1LinkLayer::init();
    _L2 = std::make_shared<L2NetworkLayer>(_myUid, std::move(_myDeviceName));
    _L3 = std::make_shared<L3TransportLayer>(_myUid);
    _L4 = std::make_shared<L4AnyfiLayer>(_myUid);

    _L1->attachIOTunnelInterface(std::move(_ioTunnel));
    _L1->attachL2Interface(_L2);

    _L2->attachL1Interface(_L1);
    _L2->attachL3Interface(_L3);

    _L3->attachL2Interface(_L2);
    _L3->attachL4Interface(_L4);

    _L4->attachL3Interface(_L3);
    _L4->attachConnectivityInterface(_connectivity);

    _L2->initialize();
    _L3->initialize();
    _L4->initialize();

    startManagement();
}

void Anyfi::Core::terminate() {
    stopManagement();

    if (_L4) {
        _L4->terminate();
    }
    if (_L3) {
        _L3->terminate();
    }
    if (_L2) {
        _L2->terminate();
    }
    if (_L1) {
        _L1->terminate();
    }
    _L4 = nullptr;
    _L3 = nullptr;
    _L2 = nullptr;
    _L1 = nullptr;


    Anyfi::CoreThread::destroy();
}

void Anyfi::Core::reset() {
    this->enqueueTask([this] {
        _L3->reset();
    });
}

Anyfi::Config::mutex_type _closestMapMutex;

const std::string Anyfi::Core::myDeviceName() {
    Anyfi::Config::lock_type guard(_propertyMutex);
    return _myDeviceName;
}

const std::string Anyfi::Core::getNetworkPerformance() {
    return _L1->getSocketPerformance();
}

void Anyfi::Core::setWrapperObject(const std::string &key, const std::string &value) {
    Anyfi::Core::getInstance()->enqueueTask([this, key, value]() {
        _connectivity->setWrapperObject(key, value);
    });
}

void Anyfi::Core::enqueueTask(std::function<void()> task) {
    if (Anyfi::CoreThread::getInstance() == nullptr) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2, "Anyfi::Core::enqueueTask",
                          "_coreThreadPool == null");
#endif
        return;
    }
    Anyfi::CoreThread::getInstance()->enqueueTask(task);
}

// =======================================
// DEPRECATED : not used
void Anyfi::Core::clearUserIpAddrMatch() {
    _userIpAddrMap.clear();
}

void Anyfi::Core::addUserIpAddrMatch(std::string userIpAddr) {
    _userIpAddrMap.insert(std::pair<std::string, time_t>(userIpAddr, 0));
}
// =======================================
void Anyfi::Core::popMatchRun(std::queue<std::pair<std::string, time_t>>* matchRun,
                              std::unordered_map<std::string, int32_t>* matchRunCount) {
    std::pair<std::string, time_t> toPop = matchRun->front();
    auto runCount = matchRunCount->find(toPop.first);
    if (runCount->second == 1) {
        matchRunCount->erase(toPop.first);
    } else {
        runCount->second--;
    }
    matchRun->pop();
}

void Anyfi::Core::updateMatchRun(std::string addr,
                                 int32_t matchRunSize,
                                 int32_t matchRunDurationSec,
                                 std::queue<std::pair<std::string, time_t>>* matchRun,
                                 std::unordered_map<std::string, int32_t>* matchRunCount) {
    std::time_t current_time = std::time(nullptr);
    if (matchRun->size() == matchRunSize) popMatchRun(matchRun, matchRunCount);
    while (!matchRun->empty()) {
        if (matchRun->front().second + matchRunDurationSec > current_time) break;
        popMatchRun(matchRun, matchRunCount);
    }

    matchRun->push(std::pair<std::string, time_t>(addr, current_time));
    auto runCount = matchRunCount->find(addr);
    if (runCount != matchRunCount->end()) {
        runCount->second++;
    } else {
        matchRunCount->insert(std::pair<std::string, int32_t>(addr, 1));
    }
}

bool Anyfi::Core::isMatchRunQualified(std::string ipAddr,
                                      int32_t matchRunQualifySize,
                                      std::unordered_map<std::string, int32_t>* matchRunCount) {
    auto runCount = matchRunCount->find(ipAddr);
    return (runCount != matchRunCount->end() && runCount->second == matchRunQualifySize);
}


void Anyfi::Core::ouim(std::string ipAddr) {
    auto quickQueryMatch = _dnsQuickHistory.find(ipAddr);
    if (quickQueryMatch != _dnsQuickHistory.end()) {
        auto current_time = std::time(nullptr);
        if (current_time - (quickQueryMatch->second).first >= _dns_query_timeout_sec) return;
        updateMatchRun((quickQueryMatch->second).second,
                       _quick_match_run_size,
                       _quick_match_run_duration_sec,
                       &_quick_matchUserIpRun,
                       &_quick_matchUserIpRunCount);
        if (isMatchRunQualified((quickQueryMatch->second).second,
                                _quick_match_run_qualify_size,
                                &_quick_matchUserIpRunCount)) {
            _connectivity->oudm((quickQueryMatch->second).second);
            (quickQueryMatch->second).first = 0;
            _quick_matchUserIpRunCount = decltype(_quick_matchUserIpRunCount)();
            _quick_matchUserIpRun = decltype(_quick_matchUserIpRun)();
        }
        return;
    }

    // normal flow
    auto queryMatch = _dnsHistory.find(ipAddr);
    if (queryMatch == _dnsHistory.end()) return;
    auto current_time = std::time(nullptr);
    if (current_time - (queryMatch->second).first >= _dns_query_timeout_sec) return;

    updateMatchRun((queryMatch->second).second,
                   _match_run_size,
                   _match_run_duration_sec,
                   &_matchUserIpRun,
                   &_matchUserIpRunCount);
//    Anyfi::Log::error(__func__, "---------------- Match count --------------");
//    for (auto &m: _matchUserIpRunCount) {
//        Anyfi::Log::error(__func__, m.first + ": " + std::to_string(m.second));
//    }
//  Anyfi::Log::error(__func__, ipAddr + " " + (queryMatch->second).second);
    if (isMatchRunQualified((queryMatch->second).second,
                            _match_run_qualify_size,
                            &_matchUserIpRunCount)) {
        _connectivity->oudm((queryMatch->second).second);
        (queryMatch->second).first = 0;
        _matchUserIpRunCount = decltype(_matchUserIpRunCount)();
        _matchUserIpRun = decltype(_matchUserIpRun)();
    }
}

// ========================================================================================
// normal flow
void Anyfi::Core::addUserDomainMatch(std::string domain) {
    _userDomainSet.insert(domain);
}

void Anyfi::Core::clearUserDomainMatch() {
    _userDomainSet.clear();
}

// ========================================================================================
// Quick check
void Anyfi::Core::addQuickUserDomainMatch(std::string domain) {
    _userQuickDomainSet.insert(domain);
}

void Anyfi::Core::clearQuickUserDomainMatch() {
    _userQuickDomainSet.clear();
}


void Anyfi::Core::oudm(std::string domain, std::string ipAddr) {
#ifdef DOMAIN_MATCH_BLACKLIST // for url collector
    if (_userDomainSet.count(domain) == 0) {
        _dnsHistory[ipAddr] = std::make_pair(time(nullptr), domain);
    }
    return;
#endif
    if (_userDomainSet.count(domain)) {
        _dnsHistory[ipAddr] = std::make_pair(time(nullptr), domain);
    } else if (_userQuickDomainSet.count(domain)) {
        _dnsQuickHistory[ipAddr] = std::make_pair(time(nullptr), domain);
    }
}

//
// ========================================================================================
// Override interfaces : IL4AnyfiLayerForConnectivity
//
void Anyfi::Core::onNetworkHandleChanged(Anyfi::NetworkType type, net_handle_t handle) {
    if (type == Anyfi::NetworkType::WIFI) {
        _networkHandle = handle;
    } else if (type == Anyfi::NetworkType::MOBILE) {
        _mobileNetworkHandle = handle;
    }
}

void Anyfi::Core::onNetworkPreferenceChanged(std::string netPref) {
    enqueueTask([this, netPref] {
        _L4->onNetworkPreferenceChanged(netPref);
    });
}

void Anyfi::Core::connectP2PLink(Anyfi::Address addr) {

}

void Anyfi::Core::disconnectP2PLink(Anyfi::Address addr) {

}

void Anyfi::Core::connectVPNLink(int fd) {
    enqueueTask([this, fd] {
        _L4->connectVPNLink(fd);
    });
}

void Anyfi::Core::disconnectVPNLink() {
    enqueueTask([this] {
        _L4->disconnectVPNLink();
    });
}


//
// Override interfaces : IL4AnyfiLayerForConnectivity
// ========================================================================================
//
void Anyfi::Core::startManagement() {
    stopManagement();

    _managementTimerTask = Anyfi::Timer::schedule([this] {
        enqueueTask([this] {
            manage();
        });
    }, 1000, 1000);
}

void Anyfi::Core::stopManagement() {
    if (_managementTimerTask == nullptr)
        return;

    Anyfi::Timer::cancel(_managementTimerTask);
    _managementTimerTask.reset();
}

void Anyfi::Core::manage() {
    Anyfi::LogManager::getInstance()->setTrafficStatsWrapperObject();

    _L1->manage();
    _L4->manage();
//    _L4->dump();

    NetworkBufferPool::clear();
}
