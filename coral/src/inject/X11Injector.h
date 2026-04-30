#pragma once

#include <string>

class X11Injector
{
public:
    static X11Injector& getInstance();
    void typeText(const std::string& text);

    // Timing parameters (tunable) specific to X11 injection
    static constexpr int kServeIdleExitMs = 1000;
    static constexpr int kInitialSleepBeforePasteMs = 800;
    static constexpr int kKeySequenceDelayUs = 20000;
    static constexpr int kClipboardServeTimeoutMs = 3000;
    static constexpr int kClipboardPollIntervalMs = 5;

private:
    X11Injector() = default;
}; 