#include "ConfigReloadThread.h"
#include "StopRecordEvent.h"
#include "Logger.h"
#include <iostream>
#include <chrono>

ConfigReloadThread::ConfigReloadThread(
        Config& config,
        PushToTalkRecordEventThread& pttThread,
        ContinuousRecordEventThread& contThread,
        PushToTalkCmdEventThread& cmdThread,
        DoubleTapKeyDetector& doubleTapDetector,
        ConcurrentQueue<std::shared_ptr<RecorderEvent>>& recorderEventQueue,
        std::atomic<bool>& reloadFlag)
    : mConfig(config),
      mPttThread(pttThread),
      mContThread(contThread),
      mCmdThread(cmdThread),
      mDoubleTapDetector(doubleTapDetector),
      mRecorderEventQueue(recorderEventQueue),
      mReloadFlag(reloadFlag)
{
}

ConfigReloadThread::~ConfigReloadThread()
{
    stop();
}

void ConfigReloadThread::start()
{
    if (mRunning.exchange(true)) return;
    mThread = std::thread(&ConfigReloadThread::run, this);
}

void ConfigReloadThread::stop()
{
    mRunning = false;
    if (mThread.joinable())
        mThread.join();
}

void ConfigReloadThread::run()
{
    while (mRunning)
    {
        if (mReloadFlag.exchange(false, std::memory_order_acquire))
        {
            std::string oldMode       = mConfig.getTriggerMode();
            std::string oldCmdKey     = mConfig.getCmdTriggerKey();
            std::string oldTriggerKey = mConfig.getTriggerKey();

            try {
                mConfig.reload();
                Logger::getInstance().setDebugLevel(mConfig.getDebugLevel());
            } catch (const std::exception& e) {
                ERROR("Config reload failed: " + std::string(e.what()));
                continue;
            }

            std::string newMode       = mConfig.getTriggerMode();
            std::string newCmdKey     = mConfig.getCmdTriggerKey();
            std::string newTriggerKey = mConfig.getTriggerKey();

            // Keep DoubleTapKeyDetector in sync
            mDoubleTapDetector.setTriggerKey(newTriggerKey);
            mDoubleTapDetector.setCmdTriggerKey(newCmdKey);

            // --- handle trigger-mode change ---
            if (newMode != oldMode)
            {
                INFO("Trigger mode changed: " + oldMode + " -> " + newMode);

                // Ensure any in-flight recording is stopped cleanly
                mRecorderEventQueue.push(
                    std::make_shared<StopRecordEvent>("config-reload"));

                if (oldMode == "pushToTalk")
                    mPttThread.stop();
                else
                    mContThread.stop();

                if (newMode == "pushToTalk")
                    mPttThread.start();
                else
                    mContThread.start();
            }
            else if (newTriggerKey != oldTriggerKey)
            {
                // Same mode but key changed — restart the active event thread
                // so its run() re-reads the trigger key from config.
                mRecorderEventQueue.push(
                    std::make_shared<StopRecordEvent>("config-reload"));

                if (newMode == "pushToTalk")
                {
                    mPttThread.stop();
                    mPttThread.start();
                }
                else
                {
                    mContThread.stop();
                    mContThread.start();
                }
            }

            // --- handle cmd-trigger-key change ---
            if (newCmdKey != oldCmdKey)
            {
                mCmdThread.stop();
                if (!newCmdKey.empty())
                    mCmdThread.start();
            }

            INFO("Configuration reloaded successfully");
            std::cout << "CONFIG_RELOADED" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
