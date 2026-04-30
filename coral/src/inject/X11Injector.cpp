#include "X11Injector.h"
#include "Logger.h"
#include "X11WindowUtils.h"
#include "ClipboardOwner.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>

extern "C" {
#include "xdo.h"
}

X11Injector& X11Injector::getInstance()
{
    static X11Injector instance;
    return instance;
}

void X11Injector::typeText(const std::string& text)
{
    Display* display = XOpenDisplay(NULL);
    if (!display)
    {
        ERROR("Failed to open X display");
        return;
    }

    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
    ClipboardOwner clipboardOwner(display, window);

    std::this_thread::sleep_for(std::chrono::milliseconds(X11Injector::kInitialSleepBeforePasteMs));
    XFlush(display);
    XSync(display, False);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    xdo_t* xdo = xdo_new_with_opened_display(display, NULL, 0);
    if (!xdo)
    {
        ERROR("Failed to initialize xdo context");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return;
    }

    Window focused_window = 0;
    if (xdo_get_focused_window(xdo, &focused_window) != 0 || focused_window == 0)
    {
        ERROR("Failed to get focused window with xdo");
    }
    else
    {
        const bool isThisTerminalWindow = X11WindowUtils::isTerminal(display, focused_window);
        Window top_window = (isThisTerminalWindow)
                                ? X11WindowUtils::getTopLevelWindow(display, focused_window)
                                : focused_window;
        DEBUG(3, std::string("Focused window ID: ") + std::to_string(focused_window));
        DEBUG(3, std::string("top_window: ") + std::to_string(top_window));

        std::string class_name;
        XClassHint class_hints;
        if (XGetClassHint(display, top_window, &class_hints))
        {
            if (class_hints.res_class)
            {
                class_name = class_hints.res_class;
                std::transform(class_name.begin(), class_name.end(), class_name.begin(), [](unsigned char c){ return std::tolower(c); });
                DEBUG(3, "Window class (for paste selection): " + class_name);
            }
            if (class_hints.res_name) XFree(class_hints.res_name);
            if (class_hints.res_class) XFree(class_hints.res_class);
        }

        std::vector<const char*> shortcuts;
        if (isThisTerminalWindow)
        {
            bool is_xterm_like = (!class_name.empty()) && (
                class_name.find("xterm") != std::string::npos ||
                class_name.find("uxterm") != std::string::npos ||
                class_name.find("urxvt") != std::string::npos ||
                class_name.find("rxvt") != std::string::npos ||
                class_name.find("st") != std::string::npos
            );
            if (is_xterm_like)
            {
                shortcuts = { "shift+Insert" };
            }
            else
            {
                shortcuts = { "ctrl+shift+v", "shift+Insert" };
            }
        }
        else
        {
            shortcuts = { "ctrl+v" };
        }

        int delay = X11Injector::kKeySequenceDelayUs; // microseconds
        Window target_window = focused_window;
        bool usedShiftInsert = false;
        for (size_t idx = 0; idx < shortcuts.size(); ++idx)
        {
            const char* seq = shortcuts[idx];
            DEBUG(3, std::string("Sending paste shortcut '") + seq + "' to window " + std::to_string(target_window));
            xdo_send_keysequence_window(xdo, target_window, seq, delay);
            DEBUG(3, "Paste command sent");
            if (std::string(seq) == std::string("shift+Insert"))
            {
                usedShiftInsert = true;
            }
            if (idx + 1 < shortcuts.size())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
            }
        }

        XFlush(display);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        clipboardOwner.serveRequests(text,
                                     X11Injector::kClipboardServeTimeoutMs,
                                     X11Injector::kClipboardPollIntervalMs,
                                     X11Injector::kServeIdleExitMs);

        if (usedShiftInsert)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            DEBUG(3, "Post-serve: sending Right then Left to clear selection highlight");
            xdo_send_keysequence_window(xdo, target_window, "Right", delay);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            xdo_send_keysequence_window(xdo, target_window, "Left", delay);
        }
    }

    xdo_free(xdo);
    XDestroyWindow(display, window);
} 