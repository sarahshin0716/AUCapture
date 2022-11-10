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

#include "Packet.h"

#include "PacketHeader.h"


template<class P, class H>
void PacketBuilder<P, H>::headerBuilder(PacketHeaderBuilder<H> &header) {
    _headerBuilder = std::move(header);
}

template<class P, class H>
std::shared_ptr<P> PacketBuilder<P, H>::build(NetworkBufferPointer buffer) {
    // TODO : depending on NetworkBuffer impl
    return std::shared_ptr<P>();
}
