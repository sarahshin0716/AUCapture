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

#include "Log.h"
#include "Log/Processor/spdlog/spdlog.h"
#include "Common/Uuid.h"
#include "Log/Backend/LogManager.h"
#include "Config.h"

#include <string>
#include <chrono>
#include <sstream>
#include <iostream>


#define FILE_LOGGER_NAME "Anyfi_log_file"

std::shared_ptr<Anyfi::Log> Anyfi::Log::_instance = nullptr;
Anyfi::Config::mutex_type Anyfi::Log::_logMutex;
Anyfi::Config::mutex_type Anyfi::Log::_fileCreateMutex;


Anyfi::Log::ExecutionTimeLogger::ExecutionTimeLogger(const std::string &tag, bool isFileWrite /* = false */) {
    _tag = tag;
    _start = std::chrono::high_resolution_clock::now();
    _isFileWrite = isFileWrite;
}


Anyfi::Log::ExecutionTimeLogger::~ExecutionTimeLogger() {
    auto executionTime = std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::high_resolution_clock::now() - _start).count();
    double tmp = executionTime;
    std::stringstream stringExecutionTime;

    // make decimal point to 1/100 only
    unsigned int i = 1;
    while ((tmp / 10.0) >= 1) {
        i++;
        tmp /= 10.0;
    }
    stringExecutionTime.precision(i);
    stringExecutionTime << executionTime / 1000.0;

    Anyfi::Log::performance(_tag, stringExecutionTime.str(), _isFileWrite);
}


void Anyfi::Log::debug(const DebugFilter &filterTag, const std::string &tag, const std::string &message) {
    if (Log::DEBUG_FILTER_SET & filterTag) {
        auto instance = _getInstance();
        instance->_getLogger()->debug("[debug] {} {}", tag, message);

        if (instance->_platformLog) {
            std::string messageWithTag = tag + "/" + message;
            instance->_platformLog(Anyfi::Log::Level::DEBUG, messageWithTag);
        }
    }
}

void Anyfi::Log::debugDump(const DebugFilter &filterTag, const std::string &tag, uint8_t* buffer, size_t bufferLen)
{
    std::string dumped = to_hexstring(buffer, bufferLen);

    if (Log::DEBUG_FILTER_SET & filterTag) {
        auto instance = _getInstance();
        instance->_getLogger()->debug("[debug] {} {}", tag, dumped);
        if (instance->_fileLog)
            instance->_fileLog->debug("[debug] {} {}", tag, dumped);
    }
}

void Anyfi::Log::error(const std::string &tag, const std::string &message) {
    auto instance = _getInstance();
    instance->_getLogger()->error("[error] {} {}", tag, message);

    if (instance->_platformLog) {
        std::string messageWithTag = tag + "/" + message;
        instance->_platformLog(Anyfi::Log::Level::ERROR, messageWithTag);
    }
}


void Anyfi::Log::warn(const std::string &tag, const std::string &message) {
    auto instance = _getInstance();
    instance->_getLogger()->warn("[warning] {} {}", tag, message);

    if (instance->_platformLog) {
        std::string messageWithTag = tag + "/" + message;
        instance->_platformLog(Anyfi::Log::Level::WARN, messageWithTag);
    }
}


void Anyfi::Log::record(const Log::Record::Tag &recordTag, const std::string &message) {
    std::string tag = Log::Record::toString(recordTag);
    auto instance = _getInstance();
    instance->_getLogger()->info("[record] {} {}", tag, message);
#ifndef __ANDROID__
    if (instance->_fileLog)
        instance->_fileLog->info("{} {}", tag, message);
#endif
    if (instance->_fileLog)
        instance->_fileLog->info("{} {}", tag, message);
}

void Anyfi::Log::performance(std::string tag, const std::string &message, bool writeToFile /* = false */) {

    // TODO: cpu profiler, memmory profile will be implemented

    // execution time profiler
    if (tag.empty()) {
        tag = std::string(__FUNCTION__);
    }

    auto instance = _getInstance();
    instance->_getLogger()->info("[{}] excution time : {} ms\t", tag, message);


    if (writeToFile) {
//        instance->_fileLog->info("excution time : {} ms\t", message);
    }
}

Anyfi::Log::Log() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] %v");

    _fileLog = nullptr;
#ifndef __ANDROID__
    _terminalLog = spdlog::stdout_color_mt("Anyfi_terminal_logger");
#else
    _androidLog = spdlog::android_logger("Anyfi_android_logger");
#endif
}

std::shared_ptr<Anyfi::Log> Anyfi::Log::_getInstance() {
    Anyfi::Config::lock_type guard(_logMutex);
    if (_instance == nullptr) {
        _instance = std::shared_ptr<Anyfi::Log>(new Anyfi::Log());
    }
    return _instance;
}

std::shared_ptr<spdlog::logger> Anyfi::Log::_getLogger() {
    auto instance = _getInstance();
#if __ANDROID__
    return instance->_androidLog;
#else
    return instance->_terminalLog;
#endif
}

std::shared_ptr<spdlog::logger> Anyfi::Log::_createFileLogger(const std::string &loggerName) {
    //Anyfi::Config::lock_type guard(_fileCreateMutex);
    _currentFileCreateTime = std::chrono::seconds(std::time(NULL)).count();
    _currentFileName = _logFileBaseName + "_" + to_string(_currentFileCreateTime) + ".txt";
    if (spdlog::details::os::file_exists(_logPath + "/" + _currentFileName)) {
        // duplicated timestamp.
        _currentFileCreateTime += 1;
        _currentFileName = _logFileBaseName + "_" + to_string(_currentFileCreateTime) + ".txt";
    }
    auto filelogger = spdlog::basic_logger_mt(loggerName, _logPath + "/" + _currentFileName);
    filelogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] %v");
    filelogger->flush_on(spdlog::level::trace);
    return filelogger;
}

void Anyfi::Log::init(const std::string &logPath, const std::string &fileBaseName) {
    auto instance = _getInstance();
    instance->_logPath = logPath;
    instance->_logFileBaseName = fileBaseName;
    spdlog::drop(FILE_LOGGER_NAME);
    instance->_fileLog = instance->_createFileLogger(FILE_LOGGER_NAME);
}

void Anyfi::Log::flush() {
    auto instance = _getInstance();
    if (instance->_fileLog) {
        auto loggerName = instance->_fileLog->name();
        instance->_fileLog->flush();
        instance->_fileLog.reset();
        spdlog::drop(loggerName);
        instance->_fileLog = instance->_createFileLogger(FILE_LOGGER_NAME);
    }
}