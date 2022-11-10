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

#ifndef ANYFIMESH_CORE_VPNETWORK_H
#define ANYFIMESH_CORE_VPNETWORK_H

#include "../../L2NetworkLayer/NetworkInterface.h"
#include "../../L2NetworkLayer/Network.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

namespace L2 {

/**
* Device에서 발생한 패킷과 MeshNetwork 사이를 중계하기 위한 네트워크입니다.
*/
class VPNetwork : public L2::Network {
public:
    VPNetwork() {
        _L2 = nullptr;
        _vpnLinkInfo = nullptr;
    };

    /**
     * Override interfaces : L2::Network
     */
    void attachL2Interface(std::shared_ptr<L2::NetworkInterface> l2interface) override {
        _L2 = std::dynamic_pointer_cast<L2::NetworkInterfaceForVPN>(l2interface);
    }
    bool isContained(unsigned short channelId) override;
    void onRead(unsigned short channelId, const std::shared_ptr<Link>& link, NetworkBufferPointer buffer) override;
    void write(Anyfi::Address dstAddr, NetworkBufferPointer buffer) override;
    void onClose(unsigned short channelId) override;


    std::shared_ptr<ChannelInfo> getVPNChannelInfo() {
        return _vpnLinkInfo;
    }
    void setVPNLink(std::shared_ptr<ChannelInfo> channelInfo) {
        _vpnLinkInfo = channelInfo;
    }

private:
    std::shared_ptr<L2::NetworkInterfaceForVPN> _L2;

    std::shared_ptr<ChannelInfo> _vpnLinkInfo;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(VPNetwork, Init);
    FRIEND_TEST(VPNetwork, Read);
#endif

};

} // namespace L2

#endif //ANYFIMESH_CORE_VPNETWORK_H
