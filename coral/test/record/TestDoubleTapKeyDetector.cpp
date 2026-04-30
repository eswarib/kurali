#include "../../src/record/DoubleTapKeyDetector.h"
#include "../../src/record/KeyDetector.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

class MockKeyDetector : public KeyDetector {
public:
    std::atomic<bool> triggerPressed{false};
    std::atomic<bool> cmdPressed{false};
    std::string lastKeyQueried;

    bool isTriggerKeyPressed(const std::string& keyCombo) override {
        lastKeyQueried = keyCombo;
        if (keyCombo == "Ctrl" || keyCombo == "trigger")
            return triggerPressed.load();
        if (keyCombo == "Alt+x" || keyCombo == "cmd")
            return cmdPressed.load();
        return false;
    }
};

void testEmptyTriggerKeySkipsPoll() {
    MockKeyDetector mock;
    DoubleTapKeyDetector detector(mock, 300);
    // No setTriggerKey - poll should do nothing, consume should return false
    mock.triggerPressed = true;
    detector.poll();
    assert(!detector.consumeTriggerRisingEdge());
}

void testTriggerDoubleTap() {
    MockKeyDetector mock;
    DoubleTapKeyDetector detector(mock, 300);
    detector.setTriggerKey("Ctrl");
    detector.setCmdTriggerKey("Alt+x");

    // Simulate double-tap on trigger
    mock.triggerPressed = true;
    detector.poll();
    mock.triggerPressed = false;
    detector.poll();
    mock.triggerPressed = true;
    detector.poll();
    mock.triggerPressed = false;
    detector.poll();

    assert(detector.consumeTriggerRisingEdge());
    assert(!detector.consumeTriggerRisingEdge());
}

void testCmdDoubleTap() {
    MockKeyDetector mock;
    DoubleTapKeyDetector detector(mock, 300);
    detector.setTriggerKey("Ctrl");
    detector.setCmdTriggerKey("cmd");

    // Simulate double-tap on cmd (mock uses "cmd" as key for cmdPressed)
    mock.cmdPressed = true;
    detector.poll();
    mock.cmdPressed = false;
    detector.poll();
    mock.cmdPressed = true;
    detector.poll();
    mock.cmdPressed = false;
    detector.poll();

    assert(detector.consumeCmdRisingEdge());
    assert(!detector.consumeCmdRisingEdge());
}

void testSingleTapNoDoubleTap() {
    MockKeyDetector mock;
    DoubleTapKeyDetector detector(mock, 300);
    detector.setTriggerKey("Ctrl");
    detector.setCmdTriggerKey("Alt+x");

    mock.triggerPressed = true;
    detector.poll();
    mock.triggerPressed = false;
    detector.poll();
    // No second tap
    assert(!detector.consumeTriggerRisingEdge());
}

void testTriggerAndCmdIndependent() {
    MockKeyDetector mock;
    DoubleTapKeyDetector detector(mock, 300);
    detector.setTriggerKey("trigger");
    detector.setCmdTriggerKey("cmd");

    // Double-tap trigger
    mock.triggerPressed = true;
    detector.poll();
    mock.triggerPressed = false;
    detector.poll();
    mock.triggerPressed = true;
    detector.poll();
    mock.triggerPressed = false;
    detector.poll();
    assert(detector.consumeTriggerRisingEdge());
    assert(!detector.consumeCmdRisingEdge());

    // Double-tap cmd
    mock.cmdPressed = true;
    detector.poll();
    mock.cmdPressed = false;
    detector.poll();
    mock.cmdPressed = true;
    detector.poll();
    mock.cmdPressed = false;
    detector.poll();
    assert(!detector.consumeTriggerRisingEdge());
    assert(detector.consumeCmdRisingEdge());
}

int main() {
    testEmptyTriggerKeySkipsPoll();
    testTriggerDoubleTap();
    testCmdDoubleTap();
    testSingleTapNoDoubleTap();
    testTriggerAndCmdIndependent();
    std::cout << "DoubleTapKeyDetector tests passed.\n";
    return 0;
}
