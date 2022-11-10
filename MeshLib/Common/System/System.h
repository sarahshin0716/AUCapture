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

#ifndef ANYFIMESH_CORE_SYSTEM_H
#define ANYFIMESH_CORE_SYSTEM_H

#include <string>


/**
 * This main space contains system-related functions.
 */
namespace System {

enum class Platform {
    Android, iOS, iOS_Simulator,
    Darwin, Linux, Windows,
    Undefined
};

/**
 * Returns whether this process is running with root permission.
 */
bool isRoot();

/**
 * runs the give shell command.
 * Depending on the various kernel, if iOS platforms, then throw an exception, run shell command, otherwise
 * @param command given shell command
 */
std::string shell(const char* command);

/**
 * Gets a current platform.
 */
Platform currentPlatform();

} // namespace System

#endif //ANYFIMESH_CORE_SYSTEM_H
