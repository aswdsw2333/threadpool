#include "Thread_pool.h"
#include <utility>   // for std::move
#include <functional>

Thread_pool::Thread_pool(size_t threads)
    : threads_count(threads), stop(false)
{
    for (size_t i = 0; i < threads_count; ++i)
    {
        workers.emplace_back([this] {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->mtx);
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->task_queue.empty();
                        });
                    if (this->stop && this->task_queue.empty())
                        return;
                    task = std::move(this->task_queue.front());
                    this->task_queue.pop();
                }
                task();
            }
            });
    }
}

Thread_pool::~Thread_pool()
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers)
        worker.join();
}