#include "X11WindowUtils.h"
#include "Logger.h"
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <algorithm>
#include <string>
#include <vector>

extern "C" {
#include "xdo.h"
}

bool X11WindowUtils::isTerminal(Display* display, Window window)
{
    bool isTerminal = false;
    xdo_t* xdo = xdo_new_with_opened_display(display, NULL, 0);
    if (!xdo)
    {
        ERROR("Failed to initialize xdo context");
        return false;
    }

    Window topLevel = getTopLevelWindow(display, window);
    window = topLevel;

    XClassHint class_hints;
    if (XGetClassHint(display, window, &class_hints))
    {
        if (class_hints.res_class)
        {
            std::string class_name(class_hints.res_class);
            DEBUG(3, "Window class: " + class_name);
            std::transform(class_name.begin(), class_name.end(), class_name.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            const std::vector<std::string> terminal_classes = {
                "gnome-terminal", "konsole", "xterm", "terminator", "urxvt",
                "rxvt", "st", "alacritty", "kitty", "tilix"
            };

            for (const auto& term_class : terminal_classes)
            {
                if (class_name.find(term_class) != std::string::npos)
                {
                    isTerminal = true;
                    DEBUG(3, "Detected terminal window: " + class_name);
                    break;
                }
            }
            XFree(class_hints.res_name);
            XFree(class_hints.res_class);
        }
    }
    else
    {
        DEBUG(3, "Failed to get class hints for window");
        XTextProperty wm_name_prop;
        if (XGetWMName(display, window, &wm_name_prop) && wm_name_prop.value && wm_name_prop.nitems > 0)
        {
            std::string win_name(reinterpret_cast<char*>(wm_name_prop.value), wm_name_prop.nitems);
            DEBUG(3, "Window name (WM_NAME): " + win_name);
            if (win_name.find("Terminal") != std::string::npos || win_name.find("terminal") != std::string::npos)
            {
                isTerminal = true;
            }
            XFree(wm_name_prop.value);
        }
        else
        {
            DEBUG(3, "Failed to get window WM_NAME");
        }
    }
    xdo_free(xdo);
    DEBUG(3, "isTerminal: " + std::to_string(isTerminal));
    return isTerminal;
}

Window X11WindowUtils::getTopLevelWindow(Display* display, Window w)
{
    Window root, parent;
    Window* children = nullptr;
    unsigned int nchildren = 0;
    while (XQueryTree(display, w, &root, &parent, &children, &nchildren) && parent != root)
    {
        w = parent;
    }
    if (children)
    {
        XFree(children);
    }
    return w;
}


