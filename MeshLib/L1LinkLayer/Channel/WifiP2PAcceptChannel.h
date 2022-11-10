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

#ifndef ANYFIMESH_CORE_WIFIP2PACCEPTSERVERCHANNEL_H
#define ANYFIMESH_CORE_WIFIP2PACCEPTSERVERCHANNEL_H

#include "../Channel/Channel.h"

class WifiP2PAcceptChannel : public SimpleChannel {
public:
    WifiP2PAcceptChannel(unsigned short channelId, std::shared_ptr<Selector> selector)
            : SimpleChannel(channelId, selector) {};
    WifiP2PAcceptChannel(unsigned short channelId, std::shared_ptr<Selector> selector, Anyfi::Address address)
            : SimpleChannel(channelId, selector, address) {};

    void connect(Anyfi::Address address, const std::function<void(bool result)> &callback) override;
    bool open(Anyfi::Address address) override;
    void close() override;
    void onClose(std::shared_ptr<Link> link) override;
    void write(NetworkBufferPointer buffer) override;

private:
};


#endif //ANYFIMESH_CORE_WIFIP2PACCEPTSERVERCHANNEL_H
