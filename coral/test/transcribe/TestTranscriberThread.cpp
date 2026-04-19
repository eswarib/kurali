#include "../../src/transcribe/TranscriberThread.h"
#include "../../src/config/Config.h"
#include "../../src/transcribe/concurrentQueue.h"
#include "../../src/record/AudioEvent.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <fstream>

void testTranscriberThread() {
    Config config("../../src/config.json");
    ConcurrentQueue<std::shared_ptr<AudioEvent>> audioQueue;
    ConcurrentQueue<std::shared_ptr<TextEvent>> textQueue;

    // Note: This test calls the real transcriber singleton.
    TranscriberThread transcriber(config, audioQueue, textQueue);
    
    transcriber.start();

    // The real transcriber will fail if the audio file doesn't exist.
    // This is expected, and it should produce an empty text event.
    audioQueue.push(std::make_shared<AudioEvent>("nonexistent.wav"));

    // Allow time for the thread to process the event.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    transcriber.stop();

    assert(textQueue.size() == 1);
    auto textEvent = textQueue.tryPop();
    assert(textEvent.has_value());
    assert((*textEvent)->text.empty());
}

int main() {
    testTranscriberThread();
    std::cout << "TranscriberThread tests passed.\n";
    return 0;
} 