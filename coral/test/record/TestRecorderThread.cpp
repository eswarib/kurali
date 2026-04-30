#include "../../src/record/RecorderThread.h"
#include "../../src/config/Config.h"
#include "../../src/transcribe/concurrentQueue.h"
#include "../../src/record/AudioEvent.h"
#include "../../src/record/RecorderEvent.h"
#include "../../src/record/StartRecordEvent.h"
#include "../../src/record/StopRecordEvent.h"
#include <cassert>
#include <iostream>
#include <chrono>
#include <memory>
#include <string>

void testRecorderThread()
{
    Config config("test/config/test_config.json");
    ConcurrentQueue<std::shared_ptr<AudioEvent>> audioQueue;
    ConcurrentQueue<std::shared_ptr<RecorderEvent>> recorderEventQueue;

    RecorderThread recorder(config, audioQueue, recorderEventQueue);
    recorder.start();

    // Simulate key press and release via events
    std::string triggerKey = config.getTriggerKey();
    recorderEventQueue.push(std::make_shared<StartRecordEvent>(triggerKey));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    recorderEventQueue.push(std::make_shared<StopRecordEvent>(triggerKey));

    // Allow time for the thread to process the events and push to the queue
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    recorder.stop();

    assert(audioQueue.size() == 1);
    auto event = audioQueue.tryPop();
    assert(event.has_value());
    std::string fileName = (*event)->getFileName();
    assert(fileName.find("input_") != std::string::npos && fileName.find(".wav") != std::string::npos);
    assert((*event)->getTriggerKey() == triggerKey);
}

int main()
{
    testRecorderThread();
    std::cout << "RecorderThread tests passed.\n";
    return 0;
}
