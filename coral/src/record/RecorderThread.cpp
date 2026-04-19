#include "RecorderThread.h"
#include "Recorder.h"
#include "Logger.h"
#include "StartRecordEvent.h"
#include "StopRecordEvent.h"
#include "RestartRecordEvent.h"
#include "ToggleRecordEvent.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <memory>
#include <filesystem>

namespace
{
std::string generateUniqueFilename()
{
    // Get cross-platform temp directory
    std::filesystem::path baseTmp;
    try
    {
        baseTmp = std::filesystem::temp_directory_path();
    }
    catch (...)
    {
        baseTmp = std::filesystem::path("/tmp");
    }
    
    // Create coral subfolder in temp
    std::filesystem::path coralTmpDir = baseTmp / "coral";
    try
    { 
        std::filesystem::create_directories(coralTmpDir); 
    }
    catch (...)
    {
    }
    
    // Generate unique filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << "input_" << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S") << "_" << now_ms.count() << ".wav";
    
    // Return full path
    return (coralTmpDir / oss.str()).string();
}
}

RecorderThread::RecorderThread(const Config& config,
                               ConcurrentQueue<std::shared_ptr<AudioEvent>>& audioQueue,
                               ConcurrentQueue<std::shared_ptr<RecorderEvent>>& recorderEventQueue)
    : mConfig(config), mAudioQueue(audioQueue), mRecorderEventQueue(recorderEventQueue), mRunning(false)
{
}

RecorderThread::~RecorderThread()
{
    stop();
}

void RecorderThread::start()
{
    mRunning = true;
    mThread = std::thread(&RecorderThread::run, this);
}

void RecorderThread::stop()
{
    mRunning = false;
    if (mThread.joinable())
    {
        mThread.join();
    }
}

void RecorderThread::doStopRecording()
{
    std::cout << "TRIGGER_UP" << std::endl;
    std::string audioFile = generateUniqueFilename();
    Recorder::getInstance()->stopRecording(audioFile);
    DEBUG(3, "Finished recording: " + audioFile + ", triggered by: " + mActiveTriggerKey);
    mAudioQueue.push(std::make_shared<AudioEvent>(audioFile, mActiveTriggerKey));
    mIsRecording = false;
    mActiveTriggerKey.clear();
    mRecordingSource = RecordingSource::None;
}

void RecorderThread::run()
{
    const std::string triggerKey = mConfig.getTriggerKey();
    bool isPushToTalk = (mConfig.getTriggerMode() == "pushToTalk");
    INFO(std::string("RecorderThread started. Trigger mode: ") + (isPushToTalk ? "pushToTalk (hold)" : "continuous (double-tap)"));

    while (mRunning)
    {
        auto ev = mRecorderEventQueue.waitAndPopWithTimeout(std::chrono::milliseconds(100));
        if (!ev.has_value())
            continue;

        auto& e = *ev;
        RecorderEvent::Type t = e->getType();
        std::string src = e->getSource();

        if (t == RecorderEvent::Type::Start)
        {
            if (!mIsRecording)
            {
                std::cout << "TRIGGER_DOWN" << std::endl;
                Recorder::getInstance()->setAudioParams(mConfig.getAudioAmplification(), mConfig.getNoiseGateThreshold());
                Recorder::getInstance()->startRecording();
                mIsRecording = true;
                mActiveTriggerKey = src;
                mRecordingSource = (src == triggerKey) ? RecordingSource::Trigger : RecordingSource::Cmd;
            }
        }
        else if (t == RecorderEvent::Type::Stop)
        {
            if (mIsRecording && src == mActiveTriggerKey)
                doStopRecording();
        }
        else if (t == RecorderEvent::Type::Restart)
        {
            if (mIsRecording && mRecordingSource == RecordingSource::Trigger)
            {
                doStopRecording();
                std::cout << "TRIGGER_DOWN" << std::endl;
                Recorder::getInstance()->setAudioParams(mConfig.getAudioAmplification(), mConfig.getNoiseGateThreshold());
                Recorder::getInstance()->startRecording();
                mIsRecording = true;
                mActiveTriggerKey = triggerKey;
                mRecordingSource = RecordingSource::Trigger;
            }
        }
        else if (t == RecorderEvent::Type::Toggle)
        {
            if (!mIsRecording)
            {
                std::cout << "TRIGGER_DOWN" << std::endl;
                Recorder::getInstance()->setAudioParams(mConfig.getAudioAmplification(), mConfig.getNoiseGateThreshold());
                Recorder::getInstance()->startRecording();
                mIsRecording = true;
                mActiveTriggerKey = triggerKey;
                mRecordingSource = RecordingSource::Trigger;
            }
            else if (mRecordingSource == RecordingSource::Trigger)
            {
                doStopRecording();
            }
        }
    }
}

