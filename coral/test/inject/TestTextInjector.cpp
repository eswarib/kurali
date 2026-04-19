#include "../../src/inject/TextInjector.h"
#include <iostream>

int main()
{
    TextInjector::getInstance()->typeText("Hello from TextInjector test");
    std::cout << "TestTextInjector completed.\n";
    return 0;
} 