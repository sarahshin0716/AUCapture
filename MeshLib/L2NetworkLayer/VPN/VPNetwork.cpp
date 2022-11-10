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

#include "VPNetwork.h"
#include "../../Log/Frontend/Log.h"
#include "../../Log/Backend/LogManager.h"

bool L2::VPNetwork::isContained(unsigned short channelId) {
    return _vpnLinkInfo != nullptr && _vpnLinkInfo->channelId == channelId;
}

void L2::VPNetwork::onRead(unsigned short channelId, const std::shared_ptr<Link>& link, NetworkBufferPointer buffer) {
    if (_L2 != nullptr && _vpnLinkInfo != nullptr) {
        Anyfi::LogManager::getInstance()->countVpnReadBuffer(buffer->getWritePos() - buffer->getReadPos());
        _L2->onReadIPPacketFromVPN(buffer);
    }else {
        Anyfi::Log::error(__func__, "_L2 == nullptr || _vpnLinkInfo is nullptr");
    }
}

void L2::VPNetwork::write(Anyfi::Address dstAddr, NetworkBufferPointer buffer) {
    if (_L2 != nullptr && _vpnLinkInfo != nullptr) {
        Anyfi::LogManager::getInstance()->countVpnWriteBuffer(buffer->getWritePos() - buffer->getReadPos());
        _L2->writeToChannel(_vpnLinkInfo->channelId, buffer);
    }else {
        Anyfi::Log::error(__func__, "_L2 == nullptr || _vpnLinkInfo is nullptr");
    }
}

void L2::VPNetwork::onClose(unsigned short channelId) {
    if (_vpnLinkInfo != nullptr && _vpnLinkInfo->channelId == channelId) {
        _vpnLinkInfo = nullptr;
    }
}
