#include "../../src/record/AudioEvent.h"
#include <cassert>
#include <iostream>

void testAudioEvent() {
    std::string fileName = "test.wav";
    AudioEvent event(fileName);
    assert(event.getFileName() == fileName);
}

int main() {
    testAudioEvent();
    std::cout << "AudioEvent tests passed.\n";
    return 0;
} 