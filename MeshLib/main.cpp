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

#ifndef __ANDROID__

#include "L1LinkLayer/L1LinkLayer.h"
#include "L2NetworkLayer/L2NetworkLayer.h"
#include "L3TransportLayer/L3TransportLayer.h"
#include "Core.h"


class ConnectivityDummy : public IConnectivityForL4 {
public:
    std::set<Anyfi::P2PType> getAcceptableP2P() override {
        return std::set<Anyfi::P2PType>();
    }

    void openP2PGroup(Anyfi::P2PType p2PType, const std::function<void(Anyfi::P2PType type, Anyfi::Address address)> &callback) override {
    }

    void onP2PServerOpened(Anyfi::Address address, int quality) override {
    }

    void closeP2PGroup(Anyfi::P2PType p2PType) override {
    }

    void discoverP2PGroup(bool discover) override {
    }

    void disconnectP2P(Anyfi::Address address) override {
    }
};

int main() {

}

#else
int main() {
    return 0;
}
#endif
