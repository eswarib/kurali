#pragma once

#include <string>

class WaylandInjector
{
public:
    static WaylandInjector& getInstance();
    // Returns true if handled under Wayland; false to fall back to X11
    bool typeText(const std::string& text);
private:
    WaylandInjector() = default;
}; 