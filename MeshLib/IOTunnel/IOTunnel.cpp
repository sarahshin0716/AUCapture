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

#include "IOTunnel.h"


std::shared_ptr<IOTunnel> IOTunnel::_instance = nullptr;

#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
    #elif TARGET_OS_IPHONE
        #define iOS
    #endif
#endif

#ifdef iOS
    #include "IOTunnel-iOS.cpp"
#elif _WIN32
    #include "IOTunnel-windows.cpp"
#endif


std::shared_ptr<IOTunnel> IOTunnel::open() {
    if (_instance == nullptr) {
#ifdef iOS
        _instance = std::make_shared<iOSIOTunnel>();
#elif _WIN32
        _instance = std::make_shared<WindowsIOTunnel>();
#endif
    }

    return _instance;
}