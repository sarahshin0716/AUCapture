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

#ifndef ANYFIMESH_CORE_CORE_H
#define ANYFIMESH_CORE_CORE_H

#include <string>
#include <queue>
#include <set>
#include <map>
#include <unordered_set>
#include <android/multinetwork.h>

#include "Common/Concurrency/ThreadPool.h"
#include "Connectivity/IConnectivity.h"
#include "IOTunnel/IOTunnel.h"
#include "L4AnyfiLayer/IL4AnyfiLayer.h"
#include "CoreThread.h"


// Forward declarations : Layers
class L1LinkLayer;
class L2NetworkLayer;
class L3TransportLayer;
class L4AnyfiLayer;

namespace Anyfi {
    class TimerTask;

/**
 * Use this class to access the AnyfiMesh-Core library.
 */
class Core : public IL4AnyfiLayerForConnectivity {
public:
    static Core* getInstance();

    void init(UUID myUid, std::string myDeviceName, std::shared_ptr<IConnectivityForL4> connectivity, std::shared_ptr<IOTunnel> ioTunnel);
    std::shared_ptr<IL4AnyfiLayerForConnectivity> l4Interface();

    void start();

    void terminate();

    void reset();

//
// ========================================================================================
// Override interfaces : IL4AnyfiLayerForConnectivity
//
    void onNetworkPreferenceChanged(std::string netPref) override;

    void connectP2PLink(Anyfi::Address addr) override;

    void disconnectP2PLink(Anyfi::Address addr) override;

    void connectVPNLink(int fd) override;

    void disconnectVPNLink() override;

//
// Override interfaces : IL4AnyfiLayerForConnectivity
// ========================================================================================
//
private:
    Anyfi::Config::mutex_type _propertyMutex;

    UUID _myUid;
    std::string _myDeviceName;
    double _sensitivity;
    std::shared_ptr<IConnectivityForL4> _connectivity;
    std::shared_ptr<IOTunnel> _ioTunnel;

    std::shared_ptr<L1LinkLayer> _L1;
    std::shared_ptr<L2NetworkLayer> _L2;
    std::shared_ptr<L3TransportLayer> _L3;
    std::shared_ptr<L4AnyfiLayer> _L4;

    // 30 seconds
    const uint64_t _dns_query_timeout_sec = 60;

    // 1 minute
    const uint64_t _match_run_duration_sec = 60;
    const int32_t _match_run_size = 40;
    const int32_t _match_run_qualify_size = 25;
    std::unordered_map<std::string, int32_t> _matchUserIpRunCount;
    std::queue<std::pair<std::string, time_t>> _matchUserIpRun;
    std::unordered_set<std::string> _userDomainSet;
    std::unordered_map<std::string, std::pair<time_t, std::string>> _dnsHistory; // [ip]: (last_query_time, domain)

    // DEPRECATED : not used
    std::unordered_map<std::string, time_t> _userIpAddrMap;

    // quick match list
    const uint64_t _quick_match_run_duration_sec = 20;
    const int32_t _quick_match_run_size = 10;
    const int32_t _quick_match_run_qualify_size = 5;
    std::queue<std::pair<std::string, time_t>> _quick_matchUserIpRun;
    std::unordered_map<std::string, int32_t> _quick_matchUserIpRunCount;
    std::unordered_set<std::string> _userQuickDomainSet;
    std::unordered_map<std::string, std::pair<time_t, std::string>> _dnsQuickHistory; // [ip]: (last_query_time, domain)

    // 10 minutes
    const uint64_t _match_timeout_duration_sec = 10 * 60;
    int32_t _networkMode;
    net_handle_t _networkHandle, _mobileNetworkHandle;

public:
    inline double getSensitivity() { return _sensitivity; }
    inline int32_t getNetworkMode() { return _networkMode; }
    inline net_handle_t getNetworkHandle(Anyfi::NetworkType type) {
        if(type == Anyfi::NetworkType ::WIFI) {
            return _networkHandle;
        } else if (type == Anyfi::NetworkType::MOBILE) {
            return _mobileNetworkHandle;
        }
        return  0;
    }

    const std::string myDeviceName();

    const std::string getNetworkPerformance();

    void onNetworkHandleChanged(Anyfi::NetworkType type, net_handle_t handle);

    void setWrapperObject(const std::string& key, const std::string& value);

    void clearUserIpAddrMatch();
    void addUserIpAddrMatch(std::string userIpAddrList);
    void ouim(std::string im);

    void clearUserDomainMatch();
    void addUserDomainMatch(std::string domain);
    void oudm(std::string dm, std::string im);

    void clearQuickUserDomainMatch();
    void addQuickUserDomainMatch(std::string domain);

public:
    void enqueueTask(std::function<void()> task);
    void startManagement();
    void stopManagement();

private:
    void popMatchRun(std::queue<std::pair<std::string, time_t>>* mr,
                     std::unordered_map<std::string, int32_t>* mrc);
    void updateMatchRun(std::string im,
                        int32_t rs,
                        int32_t mrds,
                        std::queue<std::pair<std::string, time_t>>* mr,
                        std::unordered_map<std::string, int32_t>* mrc);
    bool isMatchRunQualified(std::string im,
                             int32_t qs,
                             std::unordered_map<std::string, int32_t>* mrc);
    void manage();

private:
    std::shared_ptr<TimerTask> _managementTimerTask;
};

} // Anyfi namespace


#endif //ANYFIMESH_CORE_CORE_H
