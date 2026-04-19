#include "WaylandDetector.h"
#include <cstdlib>

bool isWaylandSession()
{
    const char* wl = std::getenv("WAYLAND_DISPLAY");
    if (wl && *wl) return true;
    // If DISPLAY is set but WAYLAND_DISPLAY is not, likely X11 session
    return false;
} 