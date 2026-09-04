#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sf {

class ThreadPool {
public:
    explicit ThreadPool(size_t workers = 2);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void enqueue(std::function<void()> task);
    void clear();
    void shutdown();
    size_t pending() const;

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
};

} // namespace sf
