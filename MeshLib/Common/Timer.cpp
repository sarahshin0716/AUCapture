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

#include <Core.h>
#include "Log/Frontend/Log.h"
#include "Timer.h"


namespace Anyfi {

class ConcreteTimer : public Timer {
public:
    ConcreteTimer() = default;
};

std::shared_ptr<Anyfi::ConcreteTimer> Anyfi::Timer::_instance = nullptr;


//
// ========================================================================================
// Timer static methods
//
std::shared_ptr<TimerTask> Timer::schedule(const std::function<void()> &task, unsigned int delay) {
    ensureInit();

    auto obj = std::make_shared<TimerTask>(task, (uint64_t) (now() + delay));
    {
        Anyfi::Config::lock_type guard(_instance->_tasksMutex);

        _instance->_tasks.insert(obj);
    }

    return obj;
}

std::shared_ptr<TimerTask>
Timer::schedule(const std::function<void()> &task, unsigned int delay, unsigned int period) {
    if (period == 0)
        throw std::invalid_argument("Period should not be zero");

    ensureInit();

    auto obj = std::make_shared<TimerTask>(task, (uint64_t) (now() + delay), (uint64_t) period);
    {
        Anyfi::Config::lock_type guard(_instance->_tasksMutex);

        _instance->_tasks.insert(obj);
    }

    return obj;
}

void Timer::cancel(std::shared_ptr<TimerTask> task) {
    if (task == nullptr || _instance == nullptr)
        return;

    Anyfi::Config::lock_type guard(_instance->_tasksMutex);

    _instance->_tasks.erase(task);
}

void Timer::ensureInit() {
    if (_instance == nullptr)
        _instance = std::make_shared<ConcreteTimer>();
}
//
// Timer static methods
// ========================================================================================
//


Timer::Timer() {
    _timerThread = new std::thread(std::bind(&Timer::_timerThreadImpl, this));
}

Timer::~Timer() {
    _stop = true;
    if (_timerThread != nullptr) {
        _timerThread->join();
        delete _timerThread;
    }
}

void Timer::_timerThreadImpl() {
    std::stringstream ss;
    ss << "timerThread";
    pthread_setname_np(pthread_self(), ss.str().c_str());

    _stop = false;

    /**
     * Problem
     * 현재는 임시로 delay가 0이상일때만 sleep하도록 구현되어 있음.
     * 한 task 처리에 지연이 발생하면 다른 task에 영향을 미치는 문제가 있음
     *
     * TODO
     * tick 방식이 아니라, nextExecutionTime으로 구현
     */

    uint64_t start, duration;
    int64_t delay;
    try {
        while (!_stop) {
            start = now();

            std::set<std::shared_ptr<TimerTask>> tasks;
            {
                Anyfi::Config::lock_type guard(_tasksMutex);
                for (auto it = _tasks.begin(); it != _tasks.end();) {
                    if ((*it)->_next == (uint64_t) -1) {    // expired
                        it = _tasks.erase(it);
                        continue;
                    }
                    tasks.insert(*it);
                    it++;
                }
            }

            for (auto iter = tasks.begin(); iter != tasks.end(); iter++) {
                std::shared_ptr<TimerTask> task(*iter);
                Anyfi::Core::getInstance()->enqueueTask([this, start, task] {
                    if (task->tick(start))
                        task->_next = (uint64_t) -1;    // set as expired
                });
            }

            duration = now() - start;
            delay = (int64_t) (GRANULARITY_MS * 1000) - duration;
            if (delay >= 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(delay));
            }
        }
    } catch (std::exception &e) {
        Anyfi::Log::error(__func__, "Timer::_timerThreadImpl : " +
                                    std::string(e.what()));
    } catch(...) {
        Anyfi::Log::error(__func__, "Timer::_timerThreadImpl : exception!!!");
    }
}


}   // namespace Anyfi