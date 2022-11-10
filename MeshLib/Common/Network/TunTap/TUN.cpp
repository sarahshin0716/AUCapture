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

#include "TUN.h"


#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #include "TUN-darwin.cpp"
    #endif
#elif __linux__
    #include "TUN-linux.cpp"
#elif _WIN32
    #include "TUN-windows.cpp"
#endif
