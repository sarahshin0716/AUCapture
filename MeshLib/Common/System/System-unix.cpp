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

// System-unix.cpp should only be compiled on unix
#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE || TARGET_OS_MAC
        #define UNIX
    #endif
#elif __linux__ || __unix__
    #define UNIX
#endif

#ifdef UNIX


#include "System.h"

#include <unistd.h>
#include <array>
#include <iostream>
#include <memory>

bool System::isRoot() {
    return getuid() == 0;
}

std::string System::shell(const char *command) {
    std::array<char, 128> buffer{};
    std::string result;
    std::shared_ptr<FILE> pipe(popen(command, "r"), pclose);
    if (!pipe) {
        std::cerr << "System::shell failed" << std::endl;
        return "";
    }
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

#endif