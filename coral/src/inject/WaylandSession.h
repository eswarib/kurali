#pragma once

#include <string>

class WaylandSession
{
public:
    static WaylandSession& getInstance();

    // Ensure the portal session is initialized (lazy)
    bool ensureInitialized();

    // Clipboard write via portal; returns false if unsupported or failed
    bool setClipboard(const std::string& text);

    // Send Ctrl+V via portal input; returns false if unsupported or failed
    bool sendCtrlV();

    // Fallback typing via portal input; returns false if unsupported or failed
    bool typeText(const std::string& text);

private:
    WaylandSession() = default;

    bool _initialized = false;
    std::string _rdpSessionObjectPath;
    bool _rdpStarted = false;

#ifdef HAVE_LIBPORTAL
    // Forward typedefs instead of including headers here
    typedef struct _XdpPortal XdpPortal;
    typedef struct _XdpSession XdpSession;
    typedef struct _GDBusConnection GDBusConnection;
    typedef struct _GDBusProxy GDBusProxy;
    typedef struct _GVariant GVariant;
    typedef struct _GVariantType GVariantType;

    XdpPortal* _portal = nullptr;
    XdpSession* _rdpSession = nullptr;

    GDBusConnection* _sessionBus = nullptr;
    GDBusProxy* _remoteDesktopProxy = nullptr;
    GDBusProxy* _clipboardProxy = nullptr;

    // Portal session lifecycle
    bool createSession();
    bool selectDevices();
    bool startSession();

    // Shared D-Bus call helper
    bool portalCall(const std::string& interface,
                    const std::string& method,
                    GVariant* params,
                    const GVariantType* replyType,
                    int timeoutMs);

    // Build the XDG portal request object path for signal subscription
    std::string buildRequestPath(const std::string& token) const;

    // Send a single keysym press/release via RemoteDesktop portal
    bool notifyKeysym(unsigned int keysym, unsigned int state);
#endif
}; 