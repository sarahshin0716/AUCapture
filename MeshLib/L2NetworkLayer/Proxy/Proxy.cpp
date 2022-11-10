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

#include "Proxy.h"

bool L2::Proxy::operator==(const Proxy &other) const {
    return this->dstAddr() == other.dstAddr() &&
            this->channelId() == other.channelId();
}

bool L2::Proxy::operator!=(const Proxy &other) const {
    return this->channelId() != other.channelId() ||
            this->dstAddr() != other.dstAddr();
}