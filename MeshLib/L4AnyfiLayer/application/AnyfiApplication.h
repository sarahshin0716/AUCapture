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

#ifndef ANYFIMESH_CORE_ANYFIAPPLICATION_H
#define ANYFIMESH_CORE_ANYFIAPPLICATION_H

#include "../../L4AnyfiLayer/IL4AnyfiLayer.h"

// Forward declarations : L4AnyfiLayer
class L4AnyfiLayer;

namespace L4 {
class AnyfiApplication {
public:
    AnyfiApplication() = default;
    void attachL4Interface(std::shared_ptr<IL4AnyfiLayerForApplication> L4);
protected:
    std::shared_ptr<IL4AnyfiLayerForApplication> _L4 = nullptr;
};
} // namespace L4


#endif //ANYFIMESH_CORE_ANYFIAPPLICATION_H
