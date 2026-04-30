#ifndef CONFIG_RELOAD_THREAD_H
#define CONFIG_RELOAD_THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include "Config.h"
#include "RecorderEvent.h"
#include "concurrentQueue.h"
#include "PushToTalkRecordEventThread.h"
#include "ContinuousRecordEventThread.h"
#include "PushToTalkCmdEventThread.h"
#include "DoubleTapKeyDetector.h"

/**
 * Watches for a config-reload signal (SIGUSR1 via atomic flag) and
 * re-reads the config file.  When the trigger mode changes it
 * stops / starts the appropriate event-producer threads.
 */
class ConfigReloadThread
{
public:
    ConfigReloadThread(Config& config,
                       PushToTalkRecordEventThread& pttThread,
                       ContinuousRecordEventThread& contThread,
                       PushToTalkCmdEventThread& cmdThread,
                       DoubleTapKeyDetector& doubleTapDetector,
                       ConcurrentQueue<std::shared_ptr<RecorderEvent>>& recorderEventQueue,
                       std::atomic<bool>& reloadFlag);
    ~ConfigReloadThread();
    void start();
    void stop();

private:
    void run();

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    Config& mConfig;
    PushToTalkRecordEventThread& mPttThread;
    ContinuousRecordEventThread& mContThread;
    PushToTalkCmdEventThread& mCmdThread;
    DoubleTapKeyDetector& mDoubleTapDetector;
    ConcurrentQueue<std::shared_ptr<RecorderEvent>>& mRecorderEventQueue;
    std::atomic<bool>& mReloadFlag;
};

#endif // CONFIG_RELOAD_THREAD_H
