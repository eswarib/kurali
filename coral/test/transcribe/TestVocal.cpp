#include "../../src/transcribe/concurrentQueue.h"
#include "../../src/record/AudioEvent.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <string>

// Mock transcriber: returns file name as text
std::string mockTranscribe(const std::string& fileName) {
    return "Transcribed: " + fileName;
}

// Mock injector: just checks the text
void mockInject(const std::string& text, std::string& output) {
    output = text;
}

void testPipeline() {
    ConcurrentQueue<std::shared_ptr<AudioEvent>> audioQueue;
    ConcurrentQueue<std::string> textQueue;
    std::string injectedText;
    std::atomic<bool> running{true};

    // Producer: push audio event
    std::thread producer([&]() {
        audioQueue.push(std::make_shared<AudioEvent>("file1.wav"));
    });

    // Transcriber: pop audio, push text
    std::thread transcriber([&]() {
        auto audioEvent = audioQueue.waitAndPop();
        std::string text = mockTranscribe(audioEvent->getFileName());
        textQueue.push(text);
    });

    // Injector: pop text, inject
    std::thread injector([&]() {
        std::string text = textQueue.waitAndPop();
        mockInject(text, injectedText);
    });

    producer.join();
    transcriber.join();
    injector.join();

    assert(injectedText == "Transcribed: file1.wav");
}

int main() {
    testPipeline();
    std::cout << "TestVocal integration test passed.\n";
    return 0;
} 