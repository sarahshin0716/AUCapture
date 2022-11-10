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

#ifndef ANYFIMESH_CORE_P2PCHANNEL_H
#define ANYFIMESH_CORE_P2PCHANNEL_H

#include "Channel.h"

class P2PChannel : public ComplexChannel {
public:
    P2PChannel(unsigned short channelId, std::shared_ptr<Selector> selector, Anyfi::Address address, unsigned char linkCount)
            : ComplexChannel(channelId, std::move(selector), address, linkCount) { }
};

#endif //ANYFIMESH_CORE_P2PCHANNEL_H
