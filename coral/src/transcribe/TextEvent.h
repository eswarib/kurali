#ifndef TEXT_EVENT_H
#define TEXT_EVENT_H

#include <chrono>
#include <string>

class TextEvent
{
public:
    explicit TextEvent(std::string t, std::chrono::steady_clock::time_point enqueued_at)
        : text_(std::move(t)), enqueued_at_(enqueued_at)
    {
    }
    const std::string& getText() const
    {
        return text_;
    }
    void setText(const std::string& t)
    {
        text_ = t;
    }
    std::chrono::steady_clock::time_point getEnqueuedAt() const
    {
        return enqueued_at_;
    }

private:
    std::string text_;
    std::chrono::steady_clock::time_point enqueued_at_;
};

#endif // TEXT_EVENT_H
