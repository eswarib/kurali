#ifndef CONCURRENTQUEUE_H
#define CONCURRENTQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <optional>
#include <chrono>

// Thread-safe queue for inter-thread communication
// Follows RAII and standard C++17 guidelines

template<typename T>
class ConcurrentQueue
{
public:
    ConcurrentQueue() = default;
    ~ConcurrentQueue() = default;

    // Disable copy
    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

    // Allow move
    ConcurrentQueue(ConcurrentQueue&&) = default;
    ConcurrentQueue& operator=(ConcurrentQueue&&) = default;

    // Add an item to the queue
    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        condVar_.notify_one();
    }

    // Wait and pop an item from the queue
    T waitAndPop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condVar_.wait(lock, [this]
        {
            return !queue_.empty();
        });
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // Wait for an item with timeout; returns std::nullopt if timeout expires
    template<typename Rep, typename Period>
    std::optional<T> waitAndPopWithTimeout(const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!condVar_.wait_for(lock, timeout, [this] { return !queue_.empty(); }))
            return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // Try to pop an item, returns std::nullopt if empty
    std::optional<T> tryPop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
        {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // Check if the queue is empty
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    // Get the size of the queue
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
    std::queue<T> queue_;
};

#endif // CONCURRENTQUEUE_H