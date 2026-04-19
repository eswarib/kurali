#include "../../src/record/KeyDetector.h"
#include <cassert>
#include <iostream>

void testKeyDetector() {
    KeyDetector detector;
    // The stub implementation always returns false, so this should pass.
    assert(!detector.isTriggerKeyPressed("Fn"));
    assert(!detector.isTriggerKeyPressed("any_other_key"));
}

int main() {
    testKeyDetector();
    std::cout << "KeyDetector tests passed.\n";
    return 0;
} 