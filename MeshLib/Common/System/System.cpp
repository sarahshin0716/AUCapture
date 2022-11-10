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

#include "System.h"


#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE || TARGET_OS_MAC
        #define UNIX
    #endif
#elif __linux__ || __unix__
    #define UNIX
#endif

#ifdef UNIX
    #include "System-unix.cpp"
#elif _WIN32
    #include "System-windows.cpp"
#endif


System::Platform System::currentPlatform() {
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
        return Platform::iOS_Simulator;
#elif TARGET_OS_IPHONE
        return Platform::iOS;
#elif TARGET_OS_MAC
        return Platform::Darwin;
#endif
#elif defined(ANDROID) || defined(__ANDROID__)
    return Platform::Android;
#elif _WIN32
    return Platform::Windows;
#elif __linux__
    return Platform::Linux;
#else
    return Platform::Undefined;
#endif
}
