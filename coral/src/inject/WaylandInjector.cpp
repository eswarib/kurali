#include "WaylandInjector.h"
#include "WaylandSession.h"
#include "Logger.h"
#include <cstdlib>
#include <string>

WaylandInjector& WaylandInjector::getInstance()
{
    static WaylandInjector instance;
    return instance;
}

bool WaylandInjector::typeText(const std::string& text)
{
    auto& session = WaylandSession::getInstance();
    if (!session.ensureInitialized())
    {
        const char* de = std::getenv("XDG_CURRENT_DESKTOP");
        std::string deStr = de ? de : "";
        if (!deStr.empty())
        {
            DEBUG(3, std::string("WaylandInjector: session unavailable under desktop '") + deStr + "'; if Unity/GTK backend, RemoteDesktop/Clipboard portals are typically unavailable. Falling back to X11");
        }
        else
        {
            DEBUG(3, "WaylandInjector: session unavailable; falling back to X11");
        }
        return false;
    }

    // Preferred path: clipboard + Ctrl+V
    if (session.setClipboard(text))
    {
        if (session.sendCtrlV())
        {
            DEBUG(3, "WaylandInjector: clipboard set and Ctrl+V sent");
            return true;
        }
        WARN("WaylandInjector: clipboard set but Ctrl+V failed; will try typing fallback");
    }
    else
    {
        const char* de = std::getenv("XDG_CURRENT_DESKTOP");
        std::string deStr = de ? de : "";
        if (!deStr.empty())
        {
            DEBUG(3, std::string("WaylandInjector: clipboard write not available under desktop '") + deStr + "'; if Unity/GTK backend, clipboard portal may be missing. Will type text");
        }
        else
        {
            DEBUG(3, "WaylandInjector: clipboard write not available; will type text");
        }
    }

    // Fallback: type the text via portal input
    if (session.typeText(text))
    {
        DEBUG(3, "WaylandInjector: typed text via portal input");
        return true;
    }

    ERROR("WaylandInjector: failed to inject text under Wayland");
    return false;
} 