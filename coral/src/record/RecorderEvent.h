#ifndef RECORDER_EVENT_H
#define RECORDER_EVENT_H

#include <string>

/**
 * Base interface for recorder control events.
 * Events: start recording, stop recording, restart recording (timer/timeout).
 */
class RecorderEvent
{
public:
    enum class Type { Start, Stop, Restart, Toggle };

    virtual ~RecorderEvent() = default;

    virtual Type getType() const = 0;

    /** Source of the event: "trigger" or "cmd" (empty for Restart) */
    virtual std::string getSource() const { return mSource; }
    void setSource(const std::string& source) { mSource = source; }

protected:
    RecorderEvent() = default;
    explicit RecorderEvent(const std::string& source) : mSource(source) {}
    std::string mSource;
};

#endif // RECORDER_EVENT_H
