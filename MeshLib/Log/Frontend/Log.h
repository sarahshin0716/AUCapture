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

#ifndef ANYFIMESH_CORE_LOG_H
#define ANYFIMESH_CORE_LOG_H

#include "Config.h"
#include "../../Common/stdafx.h"
#include "../../Log/Processor/spdlog/spdlog.h"

#include <string>
#include <cstdint>
#include <mutex>

namespace Anyfi {

// Forward declarations : LogManager
class LogManager;

/**
 * frontend side Log class for whole anyfimesh-core procedures
 */
class Log {
public:
    enum Level {
        VERBOSE = 2,
        DEBUG   = 3,
        INFO    = 4,
        WARN    = 5,
        ERROR   = 6,
        ASSERT  = 7
    };

    enum DebugFilter : uint64_t {

        NONE = 0b0000,

        L1_LINK                     = 0b0001,
        L1_READ                     = 0b0010,
        L1_WRITE                    = 0b0100,
        L1                          = L1_LINK | L1_READ | L1_WRITE,                 // bit length 4

        L2_VPN                      = 0b0001 << 4,
        L2_PROXY                    = 0b0010 << 4,
        L2_MESH                     = 0b0100 << 4,
        L2                          = L2_VPN | L2_PROXY | L2_MESH,                  // bit length 8

        L3_VPN_UDP                  = 0b00000001 << 8,
        L3_VPN_TCP                  = 0b00000010 << 8,
        L3_PROXY_UDP                = 0b00000100 << 8,
        L3_PROXY_TCP                = 0b00001000 << 8,
        L3_MESH_UDP                 = 0b00010000 << 8,
        L3_MESH_RUDP                = 0b00100000 << 8,
        L3_VPN                      = L3_VPN_UDP | L3_VPN_TCP,
        L3_PROXY                    = L3_PROXY_UDP | L3_PROXY_TCP,
        L3_MESH                     = L3_MESH_UDP | L3_MESH_RUDP,
        L3                          = L3_VPN | L3_PROXY | L3_MESH,                  // bit length 16

        L4_ANYFI_PACKET             = 0b0001 << 16,
        L4_SESSION_TRANSITIONER     = 0b0010 << 16,
        L4                          = L4_ANYFI_PACKET | L4_SESSION_TRANSITIONER,    // bit length 20

        MESH_GRAPH                  = 0b0001 << 20,
        MESH_PACKET                 = 0b0010 << 20,
        MESH                        = MESH_GRAPH | MESH_PACKET,                     // bit length 24

        LOG_LOCAL                   = 0b0001 << 24,
        LOG_SERVER                  = 0b0010 << 24,
        LOG                         = (LOG_LOCAL | LOG_SERVER),                        // bit length 28

        ALL = L1 | L2 | L3 | L4 | MESH,

    };

    // for debug filtering
//    uint64_t static const DEBUG_FILTER_SET = L1 | L2 | L3 | L4;
    uint64_t static const DEBUG_FILTER_SET = 0;

    class Record {
    public:
        enum class Tag {
            PLATFORM,
            CORE,
        };

        static std::string toString(Anyfi::Log::Record::Tag tag) {
            switch (tag) {
                case Anyfi::Log::Record::Tag::PLATFORM:
#ifdef __ANDROID__
                    return "Android";
#else
                    return "Platform";
#endif
                case Anyfi::Log::Record::Tag::CORE:
                    return "Core";
                default:
                    return "invalid record tag";
            }
        }
    };

    class ExecutionTimeLogger {
    public:

        //  TODO: cpu and memory profiler will be implemented additionally

        /**
         * constructor
         * get name or tag from parameter and get time when it is created
         * @param tag tag is the name, use __FUNCTION__, if you want to get a name of a function where you code
         */
        ExecutionTimeLogger(const std::string &tag, bool isFileWrite = false);


        /**
         * destructor
         * calculate duration time between construction time and destruction time
         */
        ~ExecutionTimeLogger();

    private:
        std::string _tag;
        std::chrono::high_resolution_clock::time_point _start;
        bool _isFileWrite;
    };

    /**
     * 로그 파일 경로 세팅
     * @param logPath
     */
    static void init(const std::string &logPath, const std::string &fileBaseName);

    /**
     * 로그 메세지를 플랫폼(Android, iOS 등)으로 보내기 위한 std::function
     * @param platformLog
     */
    static void setPlatformLog(const std::function<void(int logLevel, const std::string &message)> &platformLog) {
        _getInstance()->_platformLog = platformLog;
    }

    /**
     * log for debugging
     * it checks DebugFilter filterTag and DEBUG_FILTER_SET. If it's true, log debug, do nothing, otherwise
     * @param filterTag filters in DebugFilter
     * @param tag log tag, this could be same as with filterTag, or customize
     * @param message log message
     */
    static void debug(const DebugFilter &filterTag, const std::string &tag, const std::string &message);

    static void debugDump(const DebugFilter &filterTag, const std::string &tag, uint8_t* buffer, size_t bufferLen);


    /**
     * log for errors
     * It logs on terminal and file simultaneously.
     * @param tag log tag
     * @param message error message
     */
    static void error(const std::string &tag, const std::string &message);


    /**
     * log for warnings
     * @param tag log tag
     * @param message warning message
     */
    static void warn(const std::string &tag, const std::string &message);


    /**
     * log for server, statitics data, app monitoring, etc.
     * It logs on terminal and file simultaneously.
     * for catergorized data, recordTag and recordMessage are declared in
     * namespace Anyfi::Log::Record
     * If you need any other data, you can simply add a recordTag or a recordMessage on it.
     * @param recordTag Anyfi::Log::Record enum class Tag
     * @param recordMessage Anyfi::Log::Record enum class Message
     */
    static void record(const Log::Record::Tag &recordTag, const std::string &message);


    /**
     * show estimated performance time
     * get exectution time, cpu performance time and memory performance time values from PerformanceProfiler
     * and show the estimated performance time in seconds.
     * @param tag user defined tag, if it's empty, then name of this function will be assigned.
     * @param message estimated performance time
     * @param writeToFile if true, write to file and log on terminal, log on terminal only, otherwise
     */
    static void performance(std::string tag, const std::string &message, bool writeToFile);

    static std::string currentLogFileName() { return _getInstance()->_currentFileName; }
    static long long int currentLogFileCreateTime() { return _getInstance()->_currentFileCreateTime; }
private:
    friend class LogManager;

    static std::shared_ptr<Anyfi::Log> _instance;
    static Anyfi::Config::mutex_type _logMutex;
    static Anyfi::Config::mutex_type _fileCreateMutex;


    std::shared_ptr<spdlog::logger> _fileLog;     // for logging on file
    std::shared_ptr<spdlog::logger> _terminalLog; // for logging on terminal
    std::shared_ptr<spdlog::logger> _androidLog;  // for logging on android

    std::function<void(int level, const std::string &message)> _platformLog = nullptr; // send log message to platform;

    std::string _logPath = "";
    std::string _logFileBaseName = "";
    std::string _currentFileName = "";
    long long int _currentFileCreateTime;

    std::shared_ptr<spdlog::logger> _createFileLogger(const std::string &loggerName);

    /**
     * log format set on constructor call (to be implemented)
     * default tag is logTags::DEFAULT
     * implemented by singleton pattern
     *
     * 생성자 호출시 log format 설정 (구현 예정)
     * 싱글톤으로 구현
     */
    Log();

    /**
     * get Log class's singleton object
     * @return its singleton object
     */
    static std::shared_ptr<Log> _getInstance();


    /**
     * get logger's pointer
     * depends on the platform, it calls different logger networkMode. stdout, android, etc.
     * @return return its logger type
     */
    static std::shared_ptr<spdlog::logger> _getLogger();

    /**
     * Log file flush
     */
    static void flush();
};  // class Log

}   // namespace Anyfi

#endif //ANYFIMESH_CORE_LOG_H
