#include "util/ThreadPool.hpp"

namespace sf {

ThreadPool::ThreadPool(size_t workers)
{
    if (workers == 0)
        workers = 1;
    workers_.reserve(workers);
    for (size_t i = 0; i < workers; ++i)
        workers_.emplace_back([this] { workerLoop(); });
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::workerLoop()
{
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty())
                return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (...) {
            // Swallow to keep worker alive
        }
    }
}

void ThreadPool::enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_)
            return;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<std::function<void()>> empty;
    tasks_.swap(empty);
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_)
            return;
        stop_ = true;
        std::queue<std::function<void()>> empty;
        tasks_.swap(empty);
    }
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable())
            w.join();
    }
    workers_.clear();
}

size_t ThreadPool::pending() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

} // namespace sf
