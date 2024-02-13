#pragma once

#include "ThreadPool.h"
#include <iostream>

class TaskQueue
{
    int thread_num;
    const int max_task_count;
    ThreadPool pool;

    std::vector<bool> stop;
    std::vector<std::queue<std::function<void()>>> tasks;
    std::vector<std::pair<std::mutex, std::condition_variable>> locks;

public:
    TaskQueue(int thread_num, int max_task_count);

    ~TaskQueue();

    template <class F, class... Args>
    auto enqueue(int key, F &&f, Args &&...args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

};

TaskQueue::TaskQueue(int thread_num, int max_task_count)
    : thread_num(thread_num), max_task_count(max_task_count / thread_num), pool(thread_num), stop(thread_num, false), tasks(thread_num), locks(thread_num)
{
    for (size_t i = 0; i < thread_num; i++)
    {
        int j = i;
        pool.enqueue([this](int id){
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->locks[id].first);
                    this->locks[id].second.wait(lock, [&]()
                    {
                        return this->stop[id] || !this->tasks[id].empty(); 
                    });

                    if (this->stop[id] && this->tasks[id].empty())
                        return;

                    task = std::move(this->tasks[id].front());
                    this->tasks[id].pop();
                }
                task();
            }
        }, j);
    }
}

TaskQueue::~TaskQueue()
{
    for (size_t i = 0; i < thread_num; i++)
    {
        {
            std::unique_lock<std::mutex> lock(locks[i].first);
            stop[i] = true;
        }
        locks[i].second.notify_all();
    }
}

template <class F, class... Args>
auto TaskQueue::enqueue(int key, F &&f, Args &&...args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    int pos = key % thread_num;

    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(locks[pos].first);

        if (stop[pos])
            throw std::runtime_error("enqueue on stopped TaskQueue");
        else if (tasks.size() >= max_task_count)
            throw std::runtime_error("too many tasks");
        tasks[pos].emplace([task]()
                           { (*task)(); });
    }
    locks[pos].second.notify_one();
    return res;
}