#include "PushToTalkCmdEventThread.h"
#include "StartRecordEvent.h"
#include "StopRecordEvent.h"
#include <chrono>

static constexpr int HOLD_THRESHOLD_MS = 500;
static constexpr int POLL_INTERVAL_MS = 50;

PushToTalkCmdEventThread::PushToTalkCmdEventThread(const Config& config,
                                                   KeyDetector& keyDetector,
                                                   ConcurrentQueue<std::shared_ptr<RecorderEvent>>& eventQueue)
    : mConfig(config), mKeyDetector(keyDetector), mEventQueue(eventQueue)
{
}

PushToTalkCmdEventThread::~PushToTalkCmdEventThread()
{
    stop();
}

void PushToTalkCmdEventThread::start()
{
    if (mRunning.exchange(true)) return;
    mThread = std::thread(&PushToTalkCmdEventThread::run, this);
}

void PushToTalkCmdEventThread::stop()
{
    mRunning = false;
    if (mThread.joinable())
        mThread.join();
}

void PushToTalkCmdEventThread::run()
{
    const std::string cmdKey = mConfig.getCmdTriggerKey();
    if (cmdKey.empty()) return;

    using Clock = std::chrono::steady_clock;
    Clock::time_point pressStartTime;
    bool holdingKey = false;
    bool recordingStarted = false;

    while (mRunning)
    {
        bool pressed = mKeyDetector.isTriggerKeyPressed(cmdKey);

        if (pressed && !mPrevCmdPressed)
        {
            pressStartTime = Clock::now();
            holdingKey = true;
            recordingStarted = false;
        }
        else if (!pressed && mPrevCmdPressed)
        {
            if (recordingStarted)
                mEventQueue.push(std::make_shared<StopRecordEvent>(cmdKey));
            holdingKey = false;
            recordingStarted = false;
        }

        if (holdingKey && !recordingStarted)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - pressStartTime).count();
            if (elapsed >= HOLD_THRESHOLD_MS)
            {
                mEventQueue.push(std::make_shared<StartRecordEvent>(cmdKey));
                recordingStarted = true;
            }
        }

        mPrevCmdPressed = pressed;
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
}
