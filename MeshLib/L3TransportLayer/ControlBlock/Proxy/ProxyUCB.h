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

#ifndef ANYFIMESH_CORE_PROXYUCB_H
#define ANYFIMESH_CORE_PROXYUCB_H

#include "ProxyControlBlock.h"


class ProxyUCB : public ProxyControlBlock {
public:
    ProxyUCB();

    explicit ProxyUCB(ControlBlockKey key);

    unsigned short expireTimerTicks() const { return _expireTimerTicks; }
    void addExpireTimerTicks() { _expireTimerTicks++; }
    void clearExpireTimerTicks() { _expireTimerTicks = 0; }

private:
    unsigned short _expireTimerTicks = 0;
};


#endif //ANYFIMESH_CORE_PROXYUCB_H
