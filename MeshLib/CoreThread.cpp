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

#include "CoreThread.h"

#include "L1LinkLayer/L1LinkLayer.h"
#include "L2NetworkLayer/L2NetworkLayer.h"
#include "L3TransportLayer/L3TransportLayer.h"
#include "L4AnyfiLayer/L4AnyfiLayer.h"
#include "../../../src/main/cpp/Connectivity.h"

static Anyfi::CoreThread *_instance = nullptr;

Anyfi::CoreThread* Anyfi::CoreThread::create(const std::string& name, size_t count) {
    assert(_instance == nullptr);
    _instance = new Anyfi::CoreThread(name, count);
    return _instance;
}

void Anyfi::CoreThread::destroy() {
    if (_instance != nullptr) {
        delete _instance;
        _instance = nullptr;
    }
}

void Anyfi::CoreThread::enqueueTask(std::function<void()> task)
{
    assert(_instance != nullptr);

    _instance->enqueue(task);
}

Anyfi::CoreThread* Anyfi::CoreThread::getInstance() {
    return _instance;
}

// constructor
Anyfi::CoreThread::CoreThread(const std::string& name, size_t threads) : shouldStop(false) {
    this->name = name;
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this, i] {
            std::stringstream ss;
            ss << this->name << "-" << i;
            char threadName[16];
            strncpy(threadName, ss.str().c_str(), 15); threadName[15] = 0;
            pthread_setname_np(pthread_self(), threadName);

            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock,
                                         [this] {
                                             return this->shouldStop || !this->tasks.empty();
                                         });
                    if (this->shouldStop && this->tasks.empty()) {
                        break;
                    }

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }

            Connectivity::getInstance()->detachCurrentThread();
        });
    }
}

// learn new work item to the pool
template<class F, class... Args>
auto Anyfi::CoreThread::enqueue(F &&f, Args &&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (!shouldStop) {
            tasks.emplace([task]() { (*task)(); });
        }
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
Anyfi::CoreThread::~CoreThread() {
    stop();
}

void Anyfi::CoreThread::stop()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        shouldStop = true;
    }
    condition.notify_all();

    try {
        for (std::thread &worker: workers)
            worker.join();
    } catch (std::exception &ignored) {}
}
