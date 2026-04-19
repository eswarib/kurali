#include "../../src/inject/A11yInjector.h"
#include <iostream>

int main()
{
    bool ok = A11yInjector::getInstance().typeText("Hello from AT-SPI");
    std::cout << "A11yInjector typeText returned: " << (ok ? "true" : "false") << "\n";
    std::cout << "TestA11yInjector completed.\n";
    return 0;
} 