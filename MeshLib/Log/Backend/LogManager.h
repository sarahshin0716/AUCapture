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

#ifndef ANYFIMESH_CORE_LOGMANAGER_H
#define ANYFIMESH_CORE_LOGMANAGER_H

#include <fstream>

#include "../../L1LinkLayer/Channel/Channel.h"
#include "../../Common/Network/Address.h"
#include "../../Common/Uuid.h"
#include "../../L1LinkLayer/Channel/Packet/P2PLinkHandshakePacket.h"
#include "../../L2NetworkLayer/Mesh/MeshPacket.h"
#include "../../Connectivity/IConnectivity.h"

/**
 * Log file expire timer (12 Hour).
 */
#define LOG_FILE_EXPIRE_TIME 60 * 60 * 12
#define LOG_FILE_MAX_COUNT 2

namespace Anyfi {

class LogManager {
public:
    class Record {
    public:
        void socketCreateFailed(int fd);

        void wifiP2PHandshakeLinkConnect(bool result);
        void wifiP2POpenReadLink(Anyfi::Address address);
        void wifiP2PHandshakeWrite(P2PLinkHandshakePacket::Type type);
        void wifiP2PHandshakeRead(P2PLinkHandshakePacket::Type type);

        void meshChannelAccepted(ChannelInfo channelInfo);
        void meshHandshakeWrite(MeshPacket::Type type, std::string message = "");
        void meshHandshakeRead(MeshPacket::Type type, std::string message = "");
        void meshNeighborEstablished(UUID uid, std::string message = "");
        void meshGraphUpdateRead(UUID uid, std::string message = "");
        void meshRetransmissionWrite(MeshPacket::Type type);
        void meshRetransmissionFailed(UUID uid, int meshTimerIdx);

        void connectVpnLinkResult(int fd, bool success);
        void disconnectVpnLink(int fd);
        void disconnectVpnThread();
        void disconnectVpnLinkResult(bool success);
        void vpnLinkWriteError(int bytes, int writtenBytes);

        void networkPreferenceChanged(std::string netPref);

        void fromPlatform(const std::string &message);
    };
    static LogManager* getInstance();

    void init(Anyfi::UUID uuid, std::string deviceName, const std::string &logPath);
    std::shared_ptr<Record> record() { return _record; };
    void checkRefresh();
    std::vector<std::string> _getLogFileList();
    void clear();
    void recordNetworkTraffic();

    void countVpnWriteBuffer(__uint64_t bufferSize);
    void countVpnReadBuffer(__uint64_t bufferSize);
    void countMeshWriteBuffer(__uint64_t bufferSize);
    void countMeshReadBuffer(__uint64_t bufferSize);
    void countProxyWriteBuffer(__uint64_t bufferSize);
    void countProxyReadBuffer(__uint64_t bufferSize);

    void setTrafficStatsWrapperObject();
private:
    class BufferTrafficCount {
    public:
        void countWriteBuffer(__uint64_t bufferSize);
        void countReadBuffer(__uint64_t bufferSize);
        __uint64_t getTotalWriteBytes();
        __uint64_t getTotalReadBytes();
        __uint64_t getTotalWriteMultiply();
        __uint64_t getTotalReadMultiply();
        void clear();
    private:
        std::mutex _readMutex;
        std::mutex _writeMutex;
        __uint64_t _totalWriteBytes;
        __uint64_t _totalReadBytes;
        __uint64_t _totalWriteMultiply;
        __uint64_t _totalReadMultiply;
    };

    BufferTrafficCount _vpnBufferTrafficCount;
    BufferTrafficCount _meshBufferTrafficCount;
    BufferTrafficCount _proxyBufferTrafficCount;

    Anyfi::UUID _myUUID;
    std::string _uid = "";
    std::string _deviceName = "";
    std::string _manufacturer = "";
    std::string _model = "";
    std::string _version = "";
    std::string _appVersion = "";
    std::string _logPath = "";
    std::shared_ptr<Anyfi::LogManager::Record> _record;

    void _flush();
    void _recordDeviceInformation();
};

}   // Anyfi namespace


#endif //ANYFIMESH_CORE_LOGMANAGER_H
