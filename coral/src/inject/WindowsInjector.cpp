#include "WindowsInjector.h"
#include "Logger.h"
#include "ClipboardOwner.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <thread>
#include <chrono>

WindowsInjector& WindowsInjector::getInstance()
{
    static WindowsInjector instance;
    return instance;
}

void WindowsInjector::typeText(const std::string& text)
{
#if defined(_WIN32)
    ClipboardOwner clipboardOwner;

    // Give focused window a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    clipboardOwner.serveRequests(text,
                                 1200,   // timeoutMs
                                 30,     // pollIntervalMs
                                 200);   // idleExitMs

    // Send Ctrl+V to paste
    INPUT inputs[4] = {};

    // Press Ctrl
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // Press V
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    // Release V
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Release Ctrl
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
#else
    (void)text;
    ERROR("WindowsInjector called on non-Windows platform");
#endif
}


