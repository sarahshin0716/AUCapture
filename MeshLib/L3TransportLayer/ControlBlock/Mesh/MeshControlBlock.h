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

#ifndef ANYFIMESH_CORE_MESHCONTROLBLOCK_H
#define ANYFIMESH_CORE_MESHCONTROLBLOCK_H

#include <utility>

#include "../../../L3TransportLayer/ControlBlock/ControlBlock.h"


class MeshControlBlock : public ControlBlock {
public:
    MeshControlBlock() : ControlBlock() {

    }

    explicit MeshControlBlock(ControlBlockKey key) : ControlBlock(std::move(key)) {

    }
};

#endif //ANYFIMESH_CORE_MESHCONTROLBLOCK_H
