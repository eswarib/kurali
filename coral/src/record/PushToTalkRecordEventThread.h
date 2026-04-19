#ifndef PUSH_TO_TALK_RECORD_EVENT_THREAD_H
#define PUSH_TO_TALK_RECORD_EVENT_THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include "Config.h"
#include "KeyDetector.h"
#include "RecorderEvent.h"
#include "concurrentQueue.h"

/**
 * Polls KeyDetector for trigger key (hold).
 * Pushes StartRecordEvent on key press, StopRecordEvent on key release.
 * Only runs when triggerMode is "pushToTalk".
 */
class PushToTalkRecordEventThread
{
public:
    PushToTalkRecordEventThread(const Config& config,
                                KeyDetector& keyDetector,
                                ConcurrentQueue<std::shared_ptr<RecorderEvent>>& eventQueue);
    ~PushToTalkRecordEventThread();
    void start();
    void stop();

private:
    void run();

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    const Config& mConfig;
    KeyDetector& mKeyDetector;
    ConcurrentQueue<std::shared_ptr<RecorderEvent>>& mEventQueue;
    bool mPrevTriggerPressed{false};
};

#endif // PUSH_TO_TALK_RECORD_EVENT_THREAD_H
