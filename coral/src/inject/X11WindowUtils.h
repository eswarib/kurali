#ifndef X11_WINDOW_UTILS_H
#define X11_WINDOW_UTILS_H

#include <X11/Xlib.h>

class X11WindowUtils {
public:
    static bool isTerminal(Display* display, Window window);
    static Window getTopLevelWindow(Display* display, Window window);
};

#endif // X11_WINDOW_UTILS_H

