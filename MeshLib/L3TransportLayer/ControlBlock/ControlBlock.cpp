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

#include "ControlBlock.h"


bool ControlBlockKey::operator==(const ControlBlockKey &other) const {
    return _srcAddress == other._srcAddress && _dstAddress == other._dstAddress;
}

bool ControlBlockKey::operator!=(const ControlBlockKey &other) const {
    return !operator==(other);
}

std::size_t ControlBlockKey::Hasher::operator()(const ControlBlockKey &k) const {
    return std::hash<std::string>()(k.srcAddress().addr()) + k.srcAddress().port()
                                    + std::hash<std::string>()(k.dstAddress().addr()) + k.dstAddress().port();
}
