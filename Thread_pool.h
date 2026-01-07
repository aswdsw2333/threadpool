#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <stdexcept>
#include <cstddef>

class Thread_pool
{
public:
    Thread_pool(size_t threads);
    ~Thread_pool();

    template<typename F, typename... Args>
    void enqueue(F&& f, Args&&... args)
    {
        // 将函数和参数绑定为无参 void() 可调用对象
        std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        {
            std::unique_lock<std::mutex> lock(mtx);
            if (stop)
                throw std::runtime_error("线程池已停止");
            task_queue.emplace(std::move(task));
        }
        condition.notify_one();
    }

private:
    std::mutex mtx;
    size_t threads_count;
    std::queue<std::function<void()>> task_queue;
    bool stop;
    std::vector<std::thread> workers;
    std::condition_variable condition;
};

#endif // THREAD_POOL_Ha once
