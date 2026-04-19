#include "ContinuousRecordEventThread.h"
#include "ToggleRecordEvent.h"
#include "RestartRecordEvent.h"
#include <chrono>

ContinuousRecordEventThread::ContinuousRecordEventThread(const Config& config,
                                                         KeyDetector& keyDetector,
                                                         DoubleTapKeyDetector& doubleTapKeyDetector,
                                                         ConcurrentQueue<std::shared_ptr<RecorderEvent>>& eventQueue)
    : mConfig(config), mKeyDetector(keyDetector), mDoubleTapKeyDetector(doubleTapKeyDetector), mEventQueue(eventQueue)
{
}

ContinuousRecordEventThread::~ContinuousRecordEventThread()
{
    stop();
}

void ContinuousRecordEventThread::start()
{
    if (mRunning.exchange(true)) return;
    mThread = std::thread(&ContinuousRecordEventThread::run, this);
}

void ContinuousRecordEventThread::stop()
{
    mRunning = false;
    if (mThread.joinable())
        mThread.join();
}

void ContinuousRecordEventThread::run()
{
    const std::string triggerKey = mConfig.getTriggerKey();
    const int recordWindowSec = mConfig.getRecordWindowSeconds();
    auto lastRestartTime = std::chrono::steady_clock::now();
    bool isRecording = false;

    while (mRunning)
    {
        mDoubleTapKeyDetector.poll();
        bool doubleTap = mDoubleTapKeyDetector.consumeTriggerRisingEdge();

        if (doubleTap)
        {
            mEventQueue.push(std::make_shared<ToggleRecordEvent>(triggerKey));
            isRecording = !isRecording;
        }

        if (isRecording)
        {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastRestartTime).count();
            if (elapsed >= recordWindowSec)
            {
                mEventQueue.push(std::make_shared<RestartRecordEvent>());
                lastRestartTime = now;
            }
        }
        else
        {
            lastRestartTime = std::chrono::steady_clock::now();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
