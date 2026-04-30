#ifndef UTILS_H
#define UTILS_H

#include <string>

class Utils
{
public:
    Utils() = delete; // Static class, no instantiation
    static std::string getHomeDir();
    static std::string expandTilde(const std::string& path);
};

#endif // UTILS_H
