#include "../../src/inject/WaylandInjector.h"
#include <iostream>

int main()
{
    bool ok = WaylandInjector::getInstance().typeText("Hello from Wayland portal");
    std::cout << "WaylandInjector typeText returned: " << (ok ? "true" : "false") << "\n";
    std::cout << "TestWaylandInjector completed.\n";
    return 0;
} 