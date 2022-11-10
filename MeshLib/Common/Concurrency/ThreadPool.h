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

#ifndef ANYFIMESH_CORE_THREADPOOL_H
#define ANYFIMESH_CORE_THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <sstream>


class ThreadPool {
public:
    ThreadPool(const std::string&, size_t);
    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(F &&f, Args &&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

private:
    // need to keep track of threads so we can join them
    std::vector<std::thread> workers;
    // the task queue
    std::queue<std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(const std::string& name, size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back([this, name, i] {
            std::stringstream ss;
            ss << name << "-" << i;
            pthread_setname_np(pthread_self(), ss.str().c_str());

            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock,
                                         [this]{ return this->stop || !this->tasks.empty(); });
                    if(this->stop && this->tasks.empty()) {
                        break;
                    }

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
        });
}

// learn new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (!stop) {
            tasks.emplace([task]() { (*task)(); });
        }
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();

    try {
        for (std::thread &worker: workers) {
            worker.join();
        }
    } catch (std::exception &ignored) {}
}

#endif //ANYFIMESH_CORE_THREADPOOL_H
