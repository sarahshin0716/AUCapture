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

#pragma once

#include <string>
#include <queue>
#include <thread>
#include <future>

namespace Anyfi {

/*
 * CoreThread
 */
class CoreThread {
public:
    static CoreThread* create(const std::string&, size_t threads);
    static void destroy();
    static void enqueueTask(std::function<void()> task);

    static CoreThread* getInstance();

    CoreThread(const std::string& name, size_t);
    ~CoreThread();

    template<class F, class... Args>
    auto enqueue(F &&f, Args &&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>;

public:
    void stop();

private:
    std::string name;

    // need to keep track of threads so we can join them
    std::vector<std::thread> workers;

    // the task queue
    std::queue<std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool shouldStop;
};

} // Anyfi namespace
