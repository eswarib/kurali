#include "../../src/record/AudioEvent.h"
#include <cassert>
#include <iostream>

void testAudioEvent() {
    std::string fileName = "test.wav";
    std::string triggerKey = "Ctrl+F1";
    AudioEvent event(fileName, triggerKey);
    assert(event.getFileName() == fileName);
    assert(event.getTriggerKey() == triggerKey);
}

int main() {
    testAudioEvent();
    std::cout << "AudioEvent tests passed.\n";
    return 0;
} 