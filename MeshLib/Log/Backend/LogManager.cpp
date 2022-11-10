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

#include <thread>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <string>
#include <mutex>

#include "Core.h"
#include "LogManager.h"
#include "Config.h"
#include "LogUploadTask.h"
#include "L2NetworkLayer/Mesh/MeshTimer.h"

#include <aws/core/Aws.h>

#include "Common/json.hpp"

using namespace nlohmann;

static Anyfi::LogManager *_instance = nullptr;
// mutex for singleton double-checked lock
static Anyfi::Config::mutex_type _mutex;

Anyfi::LogManager* Anyfi::LogManager::getInstance() {
    if (_instance == nullptr) {
        Anyfi::Config::lock_type guard(_mutex);
        if (_instance == nullptr) {
            _instance = new Anyfi::LogManager();
        }
    }
    return _instance;
}

void Anyfi::LogManager::init(Anyfi::UUID uuid, std::string deviceInfo, const std::string &logPath) {
    _record = std::make_shared<Record>();
    _myUUID = uuid;
    _logPath = logPath;

    auto result = json::parse(deviceInfo);
    if (result.find("uid") != result.end()) {
        _uid = result["uid"];
    }
    if (result.find("app_version") != result.end()) {
        _appVersion = result["app_version"];
    }
    _deviceName = result["name"];
    _manufacturer = result["manufacturer"];
    _model = result["model"];
    _version = result["version"];

    _vpnBufferTrafficCount.clear();
    _proxyBufferTrafficCount.clear();
    _meshBufferTrafficCount.clear();

    _recordDeviceInformation();

    Aws::SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
    Aws::InitAPI(options);
}

void Anyfi::LogManager::clear() {
    Aws::SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
    Aws::ShutdownAPI(options);
}

void Anyfi::LogManager::recordNetworkTraffic() {
    auto vpnTotalRead = _vpnBufferTrafficCount.getTotalReadBytes();
    auto vpnTotalReadMultiply = _vpnBufferTrafficCount.getTotalReadMultiply();
    auto vpnTotalWrite = _vpnBufferTrafficCount.getTotalWriteBytes();
    auto vpnTotalWriteMultiply = _vpnBufferTrafficCount.getTotalWriteMultiply();

    json vpn;
    vpn = { { "rx", vpnTotalRead }, { "*rx", vpnTotalReadMultiply }, { "tx", vpnTotalWrite }, { "*tx", vpnTotalWriteMultiply }, };

    auto meshTotalRead = _meshBufferTrafficCount.getTotalReadBytes();
    auto meshTotalReadMultiply = _meshBufferTrafficCount.getTotalReadMultiply();
    auto meshTotalWrite = _meshBufferTrafficCount.getTotalWriteBytes();
    auto meshTotalWriteMultiply = _meshBufferTrafficCount.getTotalWriteMultiply();

    json mesh;
    mesh = { { "rx", meshTotalRead }, { "*rx", meshTotalReadMultiply }, { "tx", meshTotalWrite }, { "*tx", meshTotalWriteMultiply }, };

    auto proxyTotalRead = _proxyBufferTrafficCount.getTotalReadBytes();
    auto proxyTotalReadMultiply = _proxyBufferTrafficCount.getTotalReadMultiply();
    auto proxyTotalWrite = _proxyBufferTrafficCount.getTotalWriteBytes();
    auto proxyTotalWriteMultiply = _proxyBufferTrafficCount.getTotalWriteMultiply();

    json proxy;
    proxy = { { "rx", proxyTotalRead }, { "*rx", proxyTotalReadMultiply }, { "tx", proxyTotalWrite }, { "*tx", proxyTotalWriteMultiply }, };

    json j;
    j["vpn"] = vpn;
    j["mesh"] = mesh;
    j["proxy"] = proxy;
    std::string js = j.dump();

    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);

    _vpnBufferTrafficCount.clear();
    _proxyBufferTrafficCount.clear();
    _meshBufferTrafficCount.clear();
}

std::vector<std::string> Anyfi::LogManager::_getLogFileList() {
    std::vector<std::string> logFileList;

    auto fileList = getFilesInDirectory(_logPath + "/");
    for (const auto &fileName : fileList) {
        if (fileName.substr(fileName.find_last_of(".") + 1) != "txt")
            continue;
        if (fileName != Anyfi::Log::currentLogFileName()) {
            logFileList.push_back(fileName);
        }
    }

    return logFileList;
}

void Anyfi::LogManager::checkRefresh() {
    // Save current log file and create new one on timer expire or force
    auto currTime = std::chrono::seconds(std::time(NULL)).count();
    if (currTime - Anyfi::Log::currentLogFileCreateTime() > LOG_FILE_EXPIRE_TIME) {
        _flush();
    }

    auto logFileList = _getLogFileList();
    if (logFileList.size() > LOG_FILE_MAX_COUNT) {
        std::sort(logFileList.begin(), logFileList.end());
        for (int i = 0; i < logFileList.size() - LOG_FILE_MAX_COUNT; i++) {
            const std::string localPath = _logPath + "/" + logFileList[i];
            std::remove(localPath.c_str());
        }
    }
}

void Anyfi::LogManager::_flush() {
    Anyfi::Log::flush();
    _recordDeviceInformation();
}

void Anyfi::LogManager::_recordDeviceInformation() {
    json j;
    j["device"] = { {"uuid", _myUUID.toString()}, {"uid", _uid}, {"app_version", _appVersion}, { "name", _deviceName }, { "manufacturer", _manufacturer }, { "model", _model }, { "version", _version } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::countVpnWriteBuffer(__uint64_t bufferSize) {
    _vpnBufferTrafficCount.countWriteBuffer(bufferSize);
}

void Anyfi::LogManager::countVpnReadBuffer(__uint64_t bufferSize) {
    _vpnBufferTrafficCount.countReadBuffer(bufferSize);
}

void Anyfi::LogManager::countMeshWriteBuffer(__uint64_t bufferSize) {
    _meshBufferTrafficCount.countWriteBuffer(bufferSize);
}

void Anyfi::LogManager::countMeshReadBuffer(__uint64_t bufferSize) {
    _meshBufferTrafficCount.countReadBuffer(bufferSize);
}

void Anyfi::LogManager::countProxyWriteBuffer(__uint64_t bufferSize) {
    _proxyBufferTrafficCount.countWriteBuffer(bufferSize);
}

void Anyfi::LogManager::countProxyReadBuffer(__uint64_t bufferSize) {
    _proxyBufferTrafficCount.countReadBuffer(bufferSize);
}

void Anyfi::LogManager::setTrafficStatsWrapperObject() {
    auto vpnTotalRead = _vpnBufferTrafficCount.getTotalReadBytes();
    auto vpnTotalReadMultiply = _vpnBufferTrafficCount.getTotalReadMultiply();
    auto vpnTotalWrite = _vpnBufferTrafficCount.getTotalWriteBytes();
    auto vpnTotalWriteMultiply = _vpnBufferTrafficCount.getTotalWriteMultiply();

    json vpn;
    vpn = { { "rx", vpnTotalRead }, { "*rx", vpnTotalReadMultiply }, { "tx", vpnTotalWrite }, { "*tx", vpnTotalWriteMultiply }, };


    auto meshTotalRead = _meshBufferTrafficCount.getTotalReadBytes();
    auto meshTotalReadMultiply = _meshBufferTrafficCount.getTotalReadMultiply();
    auto meshTotalWrite = _meshBufferTrafficCount.getTotalWriteBytes();
    auto meshTotalWriteMultiply = _meshBufferTrafficCount.getTotalWriteMultiply();

    json mesh;
    mesh = { { "rx", meshTotalRead }, { "*rx", meshTotalReadMultiply }, { "tx", meshTotalWrite }, { "*tx", meshTotalWriteMultiply }, };

    auto proxyTotalRead = _proxyBufferTrafficCount.getTotalReadBytes();
    auto proxyTotalReadMultiply = _proxyBufferTrafficCount.getTotalReadMultiply();
    auto proxyTotalWrite = _proxyBufferTrafficCount.getTotalWriteBytes();
    auto proxyTotalWriteMultiply = _proxyBufferTrafficCount.getTotalWriteMultiply();

    json proxy;
    proxy = { { "rx", proxyTotalRead }, { "*rx", proxyTotalReadMultiply }, { "tx", proxyTotalWrite }, { "*tx", proxyTotalWriteMultiply }, };

    json j;
    j["vpn"] = vpn;
    j["mesh"] = mesh;
    j["proxy"] = proxy;
    std::string js = j.dump();

    Anyfi::Core::getInstance()->setWrapperObject("trafficStats", js);
}

void Anyfi::LogManager::Record::socketCreateFailed(int fd) {
    json j;
    j["socket"] = { { "type", "create" }, { "fd", to_string(fd) }, { "success", "false" } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::wifiP2PHandshakeLinkConnect(bool result) {
    json j;
    j["p2p"] = { { "type", "handshake-connect" }, { "result", result ? "success" : "fail" } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}
void Anyfi::LogManager::Record::wifiP2POpenReadLink(Anyfi::Address address) {
    json j;
    j["p2p"] = { { "type", "handshake-openreadlink" }, { "address", address.toString() } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::wifiP2PHandshakeWrite(P2PLinkHandshakePacket::Type type) {
    std::string handshakeType = (type == P2PLinkHandshakePacket::REQ) ? "Req" : "Res";
    json j;
    j["p2p"] = { { "type", "handshake-write" }, {"value", handshakeType } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::wifiP2PHandshakeRead(P2PLinkHandshakePacket::Type type) {
    std::string handshakeType = (type == P2PLinkHandshakePacket::REQ) ? "Req" : "Res";

    json j;
    j["p2p"] = { { "type", "handshake-read" }, {"value", handshakeType } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::meshChannelAccepted(ChannelInfo channelInfo) {
    json j;
    j["mesh"] = { { "type", "accept" }, {"value", channelInfo.toString() } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::meshHandshakeWrite(MeshPacket::Type type, std::string message) {
    std::string handshakeType = MeshPacket::toString(type);
    json j;
    j["mesh"] = { { "type", "handshake-write" }, { "value", handshakeType }, { "message", message } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::meshHandshakeRead(MeshPacket::Type type, std::string message) {
    std::string handshakeType = MeshPacket::toString(type);
    json j;
    j["mesh"] = { { "type", "handshake-read" }, { "value", handshakeType }, { "message", message } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::meshNeighborEstablished(UUID uid, std::string message) {
    json j;
    j["mesh"] = { { "type", "neighbor-established" }, { "uid", uid.toString() }, { "message", message } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::meshGraphUpdateRead(UUID uid, std::string message) {
    json j;
    j["mesh"] = { { "type", "graph-update" }, { "uid", uid.toString() }, { "message", message } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::meshRetransmissionWrite(MeshPacket::Type type) {
    std::string packetType = MeshPacket::toString(type);
    json j;
    j["mesh"] = { { "type", "retr-write" }, { "packetType", packetType } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::meshRetransmissionFailed(UUID uid, int meshTimerIdx) {
    std::string timerName = MeshTimer::indexToString(meshTimerIdx);
    json j;
    j["mesh"] = { { "type", "retr-write" }, {"uid", uid.toString() }, { "timerName", timerName } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::connectVpnLinkResult(int fd, bool success) {
    json j;
    j["vpn"] = { { "type", "connect-link" }, { "fd", to_string(fd) }, { "success", (success ? "true" : "false") } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::disconnectVpnLink(int fd) {
    json j;
    j["vpn"] = { { "type", "disconnect-fd" }, { "fd", to_string(fd) } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::disconnectVpnThread() {
    json j;
    j["vpn"] = { { "type", "disconnect-thread" } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::disconnectVpnLinkResult(bool success) {
    json j;
    j["vpn"] = { { "type", "disconnect-link" }, { "success", (success ? "true" : "false") } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::vpnLinkWriteError(int bytes, int writtenBytes) {
    json j;
    j["vpn"] = { { "type", "vpn-write-error" }, { "bytes", to_string(bytes) }, { "written-bytes", to_string(writtenBytes) } };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::networkPreferenceChanged(std::string netPref) {
    auto result = json::parse(netPref);
    json j;
    j["net-pref"] = { result };
    std::string js = j.dump();
    Anyfi::Log::record(Anyfi::Log::Record::Tag::CORE, js);
}

void Anyfi::LogManager::Record::fromPlatform(const std::string &message) {
    Anyfi::Log::record(Anyfi::Log::Record::Tag::PLATFORM, message);
}

void Anyfi::LogManager::BufferTrafficCount::countReadBuffer(__uint64_t bufferSize) {
    std::lock_guard<std::mutex> readGuard(_readMutex);
    __uint64_t total = _totalReadBytes;
    total += bufferSize;
    if (_totalReadBytes > total) {
        // overflow
        _totalReadMultiply += 1;
    }
    _totalReadBytes = total;
}
__uint64_t Anyfi::LogManager::BufferTrafficCount::getTotalReadBytes() {
    std::lock_guard<std::mutex> readGuard(_readMutex);
    return _totalReadBytes;
}
__uint64_t Anyfi::LogManager::BufferTrafficCount::getTotalReadMultiply() {
    std::lock_guard<std::mutex> readGuard(_readMutex);
    return _totalReadMultiply;
}

void Anyfi::LogManager::BufferTrafficCount::countWriteBuffer(__uint64_t bufferSize) {
    std::lock_guard<std::mutex> writeGuard(_writeMutex);
    __uint64_t total = _totalWriteBytes;
    total += bufferSize;
    if (_totalWriteBytes > total) {
        // overflow
        _totalWriteMultiply += 1;
    }
    _totalWriteBytes = total;
}

__uint64_t Anyfi::LogManager::BufferTrafficCount::getTotalWriteBytes() {
    std::lock_guard<std::mutex> writeGuard(_writeMutex);
    return _totalWriteBytes;
}
__uint64_t Anyfi::LogManager::BufferTrafficCount::getTotalWriteMultiply() {
    std::lock_guard<std::mutex> writeGuard(_writeMutex);
    return _totalWriteMultiply;
}

void Anyfi::LogManager::BufferTrafficCount::clear() {
    std::lock_guard<std::mutex> readGuard(_readMutex);
    std::lock_guard<std::mutex> writeGuard(_writeMutex);
    _totalWriteBytes = 0;
    _totalReadBytes = 0;
    _totalWriteMultiply = 1;
    _totalReadMultiply = 1;
}

