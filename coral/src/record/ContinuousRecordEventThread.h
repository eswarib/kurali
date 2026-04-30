#ifndef CONTINUOUS_RECORD_EVENT_THREAD_H
#define CONTINUOUS_RECORD_EVENT_THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include "Config.h"
#include "KeyDetector.h"
#include "DoubleTapKeyDetector.h"
#include "RecorderEvent.h"
#include "concurrentQueue.h"

/**
 * Polls DoubleTapKeyDetector for trigger key (double-tap toggle).
 * Runs timer for recordWindowSeconds - pushes RestartRecordEvent on timeout.
 * Only runs when triggerMode is "continuous".
 */
class ContinuousRecordEventThread
{
public:
    ContinuousRecordEventThread(const Config& config,
                               KeyDetector& keyDetector,
                               DoubleTapKeyDetector& doubleTapKeyDetector,
                               ConcurrentQueue<std::shared_ptr<RecorderEvent>>& eventQueue);
    ~ContinuousRecordEventThread();
    void start();
    void stop();

private:
    void run();

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    const Config& mConfig;
    KeyDetector& mKeyDetector;
    DoubleTapKeyDetector& mDoubleTapKeyDetector;
    ConcurrentQueue<std::shared_ptr<RecorderEvent>>& mEventQueue;
};

#endif // CONTINUOUS_RECORD_EVENT_THREAD_H
