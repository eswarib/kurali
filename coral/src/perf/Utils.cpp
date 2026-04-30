#include "Utils.h"
#include <cstdlib> // For getenv

std::string Utils::getHomeDir()
{
#if defined(_WIN32)
    const char* home = getenv("USERPROFILE");
    if (home && *home) return std::string(home);
    return "";
#else
    const char* home = getenv("HOME");
    if (home && *home) return std::string(home);
    return "";
#endif
}

std::string Utils::expandTilde(const std::string& path)
{
    if (path.empty() || path[0] != '~')
    {
        return path;
    }
    std::string home = Utils::getHomeDir();
    if (!home.empty())
    {
        return home + path.substr(1);
    }
    return path;
}
