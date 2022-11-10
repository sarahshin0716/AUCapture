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

#ifndef ANYFIMESH_CORE_MESHTIMER_H
#define ANYFIMESH_CORE_MESHTIMER_H

#include <string>
#include <cstdint>

/**
 * 재전송, 일정 주기별 동작을 위한 타이머입니다.
 */
class MeshTimer {
public:
    static const     unsigned short TIMEOUT_BASE        =  1000; // 500; // for debug
    // Neighbor가 보유한 총 타이머 갯수
    static constexpr unsigned short NEIGHBOR_TIMERS     = 3;

    // Neighbor 타이머의 Index
    static constexpr unsigned short HANDSHAKE_TIMER     = 0;
    static constexpr unsigned short UPDATEGRAPH_TIMER   = 1;
    static constexpr unsigned short HEARTBEAT_TIMER     = 2;

    struct TimeoutConfig {
        unsigned short PERIOD;
        unsigned short MAX_TIMEOUT;
    };

    static const struct TimeoutConfig TIMEOUT_HANDSHAKE;
    static const struct TimeoutConfig TIMEOUT_GRAPHUPDATE;
    static const struct TimeoutConfig TIMEOUT_HEARTBEAT;

    static std::string indexToString(int idx) {
        switch (idx) {
            case HANDSHAKE_TIMER:
                return "HANDSHAKE_TIMER";
            case UPDATEGRAPH_TIMER:
                return "UPDATEGRAPH_TIMER";
            case HEARTBEAT_TIMER:
                return "HEARTBEAT_TIMER";
            default:
                return "UNKNOWN";
        }
    }
};


#endif //ANYFIMESH_CORE_MESHTIMER_H
