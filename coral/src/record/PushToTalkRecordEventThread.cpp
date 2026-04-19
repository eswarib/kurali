#include "PushToTalkRecordEventThread.h"
#include "StartRecordEvent.h"
#include "StopRecordEvent.h"
#include <chrono>

static constexpr int HOLD_THRESHOLD_MS = 500;
static constexpr int POLL_INTERVAL_MS = 50;

PushToTalkRecordEventThread::PushToTalkRecordEventThread(const Config& config,
                                                         KeyDetector& keyDetector,
                                                         ConcurrentQueue<std::shared_ptr<RecorderEvent>>& eventQueue)
    : mConfig(config), mKeyDetector(keyDetector), mEventQueue(eventQueue)
{
}

PushToTalkRecordEventThread::~PushToTalkRecordEventThread()
{
    stop();
}

void PushToTalkRecordEventThread::start()
{
    if (mRunning.exchange(true)) return;
    mThread = std::thread(&PushToTalkRecordEventThread::run, this);
}

void PushToTalkRecordEventThread::stop()
{
    mRunning = false;
    if (mThread.joinable())
        mThread.join();
}

void PushToTalkRecordEventThread::run()
{
    const std::string triggerKey = mConfig.getTriggerKey();
    if (triggerKey.empty()) return;

    using Clock = std::chrono::steady_clock;
    Clock::time_point pressStartTime;
    bool holdingKey = false;
    bool recordingStarted = false;

    while (mRunning)
    {
        bool pressed = mKeyDetector.isTriggerKeyPressed(triggerKey);

        if (pressed && !mPrevTriggerPressed)
        {
            pressStartTime = Clock::now();
            holdingKey = true;
            recordingStarted = false;
        }
        else if (!pressed && mPrevTriggerPressed)
        {
            if (recordingStarted)
                mEventQueue.push(std::make_shared<StopRecordEvent>(triggerKey));
            holdingKey = false;
            recordingStarted = false;
        }

        if (holdingKey && !recordingStarted)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - pressStartTime).count();
            if (elapsed >= HOLD_THRESHOLD_MS)
            {
                mEventQueue.push(std::make_shared<StartRecordEvent>(triggerKey));
                recordingStarted = true;
            }
        }

        mPrevTriggerPressed = pressed;
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
}
