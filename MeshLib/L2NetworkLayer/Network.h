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

#ifndef ANYFIMESH_CORE_L2NETWORK_H
#define ANYFIMESH_CORE_L2NETWORK_H

#include "IL2NetworkLayer.h"

namespace L2 {

class Network {
public:
    ~Network() = default;

    /**
     * L2 Interface 연결
     * @param l3interface
     */
    virtual void attachL2Interface(std::shared_ptr<L2::NetworkInterface> l2interface) = 0;

    /**
     * 해당 클레스에서 입력받은 chnnel id를 관리하고 있는지 확인
     * @param chnnelId
     * @return 관리하고 있을 경우 true, 아닐 경우 false
     */
    virtual bool isContained(unsigned short channelId) = 0;

    /**
     * 링크로부터 온 버퍼 수신 처리
     * @param channelId sender
     * @param buffer NetworkBuffer
     */
    virtual void onRead(unsigned short channelId, const std::shared_ptr<Link>& link, NetworkBufferPointer buffer)=0;

    /**
     * 네트워크로 버퍼 전송
     * @param buffer
     */
    virtual void write(Anyfi::Address dstAddr, NetworkBufferPointer buffer)=0;

    /**
     * channelId 해당하는 요소 정리
     * @param channelId 닫힌 channelId
     */
    virtual void onClose(unsigned short channelId) = 0;
};

} // namespace L2

#endif //ANYFIMESH_CORE_L2NETWORK_H
