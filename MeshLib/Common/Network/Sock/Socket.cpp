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

#include "Socket.h"


#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
    #elif TARGET_OS_IPHONE
    #elif TARGET_OS_MAC
        #define POSIX
    #endif
#elif __linux__ || __unix__ || defined(_POSIX_VERSION)
    #define POSIX
#endif

#ifdef _WIN32
    #include "Socket-windows.cpp"
#elif defined(POSIX)
    #include "Socket-posix.cpp"
#endif
