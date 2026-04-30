#ifndef PUSH_TO_TALK_CMD_EVENT_THREAD_H
#define PUSH_TO_TALK_CMD_EVENT_THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include "Config.h"
#include "KeyDetector.h"
#include "RecorderEvent.h"
#include "concurrentQueue.h"

/**
 * Polls KeyDetector for cmd key (hold).
 * Pushes StartRecordEvent on key press, StopRecordEvent on key release.
 * Only runs when cmdTriggerKey is configured (not empty).
 * Cmd is always push-to-talk regardless of trigger mode.
 */
class PushToTalkCmdEventThread
{
public:
    PushToTalkCmdEventThread(const Config& config,
                             KeyDetector& keyDetector,
                             ConcurrentQueue<std::shared_ptr<RecorderEvent>>& eventQueue);
    ~PushToTalkCmdEventThread();
    void start();
    void stop();

private:
    void run();

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    const Config& mConfig;
    KeyDetector& mKeyDetector;
    ConcurrentQueue<std::shared_ptr<RecorderEvent>>& mEventQueue;
    bool mPrevCmdPressed{false};
};

#endif // PUSH_TO_TALK_CMD_EVENT_THREAD_H
