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

#ifndef ANYFIMESH_CORE_TIMER_H
#define ANYFIMESH_CORE_TIMER_H

#include <memory>
#include <functional>
#include <thread>
#include <utility>
#include <string>
#include <set>


namespace Anyfi {

using namespace std::chrono;


class TimerTask {
public:
    friend class Timer;

    TimerTask(std::function<void()> task, uint64_t time)
            : _task(std::move(task)), _next(time), _period(0) {}
    TimerTask(std::function<void()> task, uint64_t first, uint64_t period)
            : _task(std::move(task)), _next(first), _period(period) {}

    /**
     * Returns true when needs
     */
    inline bool tick(const uint64_t now) {
        if (_next <= now) {
            _task();
            if (_period == 0)
                return true;

            _next += _period;
        }
        return false;
    }

private:
    const std::function<void()> _task;
    uint64_t _next = 0;
    const uint64_t _period = 0;
};


class ConcreteTimer;
class Timer {
public:
    static const unsigned short GRANULARITY_MS = 100;

    /**
     * Schedules the specified task for execution after the specified delay.
     * The task should not take more than 10 micro-seconds.
     */
    static std::shared_ptr<TimerTask> schedule(const std::function<void()> &task, unsigned int delay);

    /**
     * Schedules the specified task for repeated fixed-delay execution, beginning after the specified delay.
     * The task should not take more than 10 micro-seconds.
     */
    static std::shared_ptr<TimerTask> schedule(const std::function<void()> &task, unsigned int delay, unsigned int period);

    static void cancel(std::shared_ptr<TimerTask> task);

    Timer(const Timer &other) = delete;
    Timer &operator=(const Timer &other) = delete;

private:
    friend class ConcreteTimer;
    Timer();
    ~Timer();
    static std::shared_ptr<ConcreteTimer> _instance;
    static inline void ensureInit();

    bool _stop = false;
    std::thread *_timerThread = nullptr;
    void _timerThreadImpl();

    inline static uint64_t now() {
        return (uint64_t) duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    Anyfi::Config::mutex_type _tasksMutex;
    std::set<std::shared_ptr<TimerTask>> _tasks;
};

}  // namespace Anyfi


#endif //ANYFIMESH_CORE_TIMER_H
