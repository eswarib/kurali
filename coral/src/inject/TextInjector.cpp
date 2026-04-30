#if defined(_WIN32)
#include "WindowsInjector.h"
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#endif

#include "TextInjector.h"
#include "Logger.h"
#if !defined(_WIN32)
#include "X11WindowUtils.h"
#include "X11Injector.h"
#include "UInputInjector.h"
#endif
#include "ClipboardOwner.h"
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>

#if !defined(_WIN32)
extern "C" 
{
#include "xdo.h"
}
#endif

TextInjector* TextInjector::sInstance = nullptr;

TextInjector* TextInjector::getInstance()
{
    if (sInstance)
    {
        return sInstance;
    }
    sInstance = new TextInjector();
    return sInstance;
}

TextInjector::~TextInjector() 
{
}

// removed local helpers; now using X11WindowUtils and ClipboardOwner

void TextInjector::typeText(const std::string& text)
{
    // Windows path
    #if defined(_WIN32)
    WindowsInjector::getInstance().typeText(text);
    return;
    #else
    // uinput first — works on both X11 and Wayland via /dev/uinput
    if (UInputInjector::getInstance().isAvailable())
    {
        if (UInputInjector::getInstance().typeText(text))
        {
            DEBUG(3, "TextInjector: injected via uinput (kernel virtual keyboard)");
            return;
        }
        DEBUG(3, "TextInjector: uinput injection failed; falling back to X11");
    }

    // X11 fallback (clipboard + Ctrl+V)
    X11Injector::getInstance().typeText(text);
    #endif
}
