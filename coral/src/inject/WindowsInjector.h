#ifndef WINDOWS_INJECTOR_H
#define WINDOWS_INJECTOR_H

#include <string>

class WindowsInjector
{
public:
    static WindowsInjector& getInstance();

    void typeText(const std::string& text);

private:
    WindowsInjector() = default;
};

#endif // WINDOWS_INJECTOR_H


