#include "../../src/record/DoubleTapDetectionFSM.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

void testSingleTapNoDoubleTap() {
    DoubleTapDetectionFSM fsm(300);
    // Single tap: key down (rising), key up
    fsm.feedKeyState(true);   // rising edge -> WaitingForSecondTap
    fsm.feedKeyState(false);  // key released
    // No second tap - consumeDoubleTap should be false
    assert(!fsm.consumeDoubleTap());
}

void testDoubleTapWithinWindow() {
    DoubleTapDetectionFSM fsm(300);
    // First tap
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    // Second tap immediately (within 300ms)
    fsm.feedKeyState(true);   // second rising edge within window -> double tap!
    fsm.feedKeyState(false);
    assert(fsm.consumeDoubleTap());
    // consumeDoubleTap clears - second call returns false
    assert(!fsm.consumeDoubleTap());
}

void testDoubleTapAfterTimeout() {
    DoubleTapDetectionFSM fsm(100);
    // First tap
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    // Wait longer than window
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // Second tap - too late, should be treated as new first tap
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    // No double-tap from first+second (they were too far apart)
    assert(!fsm.consumeDoubleTap());
}

void testDoubleTapAfterTimeoutThenValidDoubleTap() {
    DoubleTapDetectionFSM fsm(100);
    // First tap
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // Second tap becomes new first tap
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    // Third tap quickly - valid double-tap with the "second" tap
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    assert(fsm.consumeDoubleTap());
}

void testReset() {
    DoubleTapDetectionFSM fsm(300);
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    fsm.reset();
    // After reset, FSM should be in Idle - next tap is first tap
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    assert(!fsm.consumeDoubleTap());
}

void testConsumeClearsPending() {
    DoubleTapDetectionFSM fsm(300);
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    fsm.feedKeyState(true);
    fsm.feedKeyState(false);
    assert(fsm.consumeDoubleTap());
    assert(!fsm.consumeDoubleTap());
    assert(!fsm.consumeDoubleTap());
}

int main() {
    testSingleTapNoDoubleTap();
    testDoubleTapWithinWindow();
    testDoubleTapAfterTimeout();
    testDoubleTapAfterTimeoutThenValidDoubleTap();
    testReset();
    testConsumeClearsPending();
    std::cout << "DoubleTapDetectionFSM tests passed.\n";
    return 0;
}
