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

#ifndef ANYFIMESH_CORE_NEIGHBOR_H
#define ANYFIMESH_CORE_NEIGHBOR_H

#include <cstdint>
#include <string>
#include <map>

#include "../../L1LinkLayer/Link/Link.h"
#include "MeshPacket.h"
#include "MeshTimer.h"
#include "Graph/GraphInfo.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

/**
 * MeshNetwork를 통해 연결된 각 link의 연결 관리를 위한 클래스입니다.
 */
class Neighbor {
public:
    enum class Status : unsigned short {
        Closed          = 0,
        Accepted        = 1,
        HandshakeSent   = 2,
        HandshakeRecv   = 3,
        Established     = 4
    };

    struct RetransmissionCount {
    public:
        bool isEnable = false;
        short count = 0;
        short period = 0;
        short periodMax = 0;
    };

    Neighbor() {
        _retransmissionCount.resize(MeshTimer::NEIGHBOR_TIMERS);
        _graphInfo = std::make_shared<GraphInfo>();
        _status = Status::Closed;
        _channelInfo = nullptr;
        _uid = Anyfi::UUID();
    }
    Neighbor(Anyfi::UUID uid, const std::string &deviceName, const ChannelInfo &channelInfo) : Neighbor() {
        _uid = uid;
        _deviceName = deviceName;
        _channelInfo = std::make_shared<ChannelInfo>(channelInfo.channelId, channelInfo.addr);
    }

    /**
     * Setters
     */
    void write(const std::function<void(NetworkBufferPointer)> &write) { _write = write; };
    void close(const std::function<void(unsigned short channelId, bool isForce)> &close) { _close = close; }
    void timeout(const std::function<void(unsigned short channelId, unsigned short timerIdx)> &timeout) { _timeout = timeout; }
    void retransmissionFailed(const std::function<void(unsigned short channelId, unsigned short timerIdx)> &retransmissionFail) { _retransmissionFail = retransmissionFail; }
    void channelInfo(const ChannelInfo &channelInfo) { _channelInfo.reset(new ChannelInfo(channelInfo.channelId, channelInfo.addr)); };
    void status(Status status) { _status = status; };
    void uid(Anyfi::UUID uid) { _uid = uid; };
    void deviceName(const std::string &deviceName) { _deviceName = deviceName; };
    void graph(std::shared_ptr<GraphInfo> graphInfo) { _graphInfo = graphInfo; };

    /**
     * Getters
     */
    std::shared_ptr<ChannelInfo> channelInfo() const { return _channelInfo; }
    Status status() { return _status; }
    Anyfi::UUID uid() { return _uid; }
    std::string deviceName() { return _deviceName; }
    std::shared_ptr<GraphInfo> graph() { return _graphInfo; }
    std::map<uint64_t, std::shared_ptr<MeshUpdatePacket>, std::less<uint64_t>> graphUpdatePacketMap() const { return _graphUpdatePacketMap; };


    /**
     * write buffer to neighbor
     * @param buffer Data to be sent
     */
    void write(NetworkBufferPointer buffer);
    /**
     * write MeshPacket to neighbor.
     * 내부에서 NetworkBuffer로 변환 후 전송합니다.
     * @param mp
     */
    void write(std::shared_ptr<MeshPacket> mp);
    /**
     * GraphUpdatePacket을 생성&전송합니다.
     * Established가 되지 않은 상태에서 전송요청이 오는 경우를 대비하고, Res가 오지 않을 경우 재전송 요청을 하기 위하여
     * _graphUpdatePacketMap에 저장합니다.
     * @param sourceUid
     * @param graphInfo
     * @param updateTime
     */
    void writeGraphUpdate(Anyfi::UUID sourceUid, const std::shared_ptr<GraphInfo> &graphInfo,
                          const std::shared_ptr<MeshEdge> &meshEdge, uint64_t updateTime);
    /**
     * _graphUpdatePacketMap에 전송해야할 패킷이 존재하고, _status가 Established면 UpdatePacket을 전송합니다.
     */
    void sendGraphUpdatePacketInMap();
    /**
     * GraphUpdatePacket의 응답을 처리합니다.
     * @param updateTime GraphUpdatePacket의 UdpateTime
     */
    void receivedGraphUpdatePacketRes(uint64_t updateTime);

    /**
     * 링크 연결 해제 및 기타 정리
     * @param isForce 수동 해제 여부
     */
    void close(bool isForce);

    /**
     * 재전송 타이머를 동작시킵니다.
     * @param idx Timer 번호({@link MeshTimer }에 정의)
     */
    void activeTimer(unsigned short idx);
    void inactiveTimer(unsigned short idx);
    void inactiveAllTimer();
    void updateTimer(unsigned short period);

private:
    std::string _deviceName = "";
    Anyfi::UUID _uid;
    std::shared_ptr<ChannelInfo> _channelInfo;
    Status _status;
    /**
     * Mesh Handshake Req를 통해 받은 GraphInfo를 보관
     * Mesh Handshake Res가 수신될 때 MeshGraph와 병합
     */
    std::shared_ptr<GraphInfo> _graphInfo;

    /**
     * REQ를 할 때 추가, RES를 수실 할 때 제거
     * std::unodered_map<updateTime, MeshUpdatePacket Buffer>
     */
    std::map<uint64_t, std::shared_ptr<MeshUpdatePacket>, std::less<uint64_t>> _graphUpdatePacketMap;
    // retransmit count
    std::vector<RetransmissionCount> _retransmissionCount;

    std::function<void(NetworkBufferPointer buffer)> _write;
    std::function<void(unsigned short channel, bool isForce)> _close;
    std::function<void(unsigned short channel, unsigned short timerIdx)> _timeout;
    std::function<void(unsigned short channel, unsigned short timerIdx)> _retransmissionFail;

    Anyfi::Config::mutex_type _timeoutMutex;

public:
    std::vector<RetransmissionCount> getRetransmissionCount() const {
        return _retransmissionCount;
    }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(MeshNetwork, ReadMeshHandshakeReq);
#endif

};

#endif //ANYFIMESH_CORE_NEIGHBOR_H
