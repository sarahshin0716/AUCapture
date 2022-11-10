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

#include "MeshRUDPPacket.h"


unsigned short MeshRUDPPacket::size() const {
    return (unsigned short) (_buffer->size() - _startPosition);
}

std::shared_ptr<MeshRUDPHeader> MeshRUDPPacket::header() const {
    return std::make_shared<MeshRUDPHeader>(_buffer, _startPosition);
}
