#pragma once

#include <string>

class A11yInjector
{
public:
    static A11yInjector& getInstance();
    // Returns true if text was injected via AT-SPI
    bool typeText(const std::string& text);

private:
    A11yInjector() = default;
}; 