#include "WaylandSession.h"
#include "Logger.h"

#ifdef HAVE_LIBPORTAL
#include <libportal/portal.h>
#include <gio/gio.h>
#include <glib.h>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <functional>

// ── RAII helpers for GLib/GDBus resources ───────────────────────────────────

// Custom deleters for GLib types
struct GErrorDeleter   { void operator()(GError* e)   const { if (e) g_error_free(e); } };
struct GVariantDeleter { void operator()(GVariant* v) const { if (v) g_variant_unref(v); } };
struct GObjectDeleter  { void operator()(gpointer p) const { if (p) g_object_unref(p); } };

using UniqueGError   = std::unique_ptr<GError, GErrorDeleter>;
using UniqueGVariant = std::unique_ptr<GVariant, GVariantDeleter>;

// Wrap a raw GError** out-parameter: pass .out() to C calls, auto-frees on scope exit.
class GErrorGuard
{
public:
    GErrorGuard() = default;
    ~GErrorGuard() { if (mError) g_error_free(mError); }

    GError** out()            { return &mError; }
    explicit operator bool()  const { return mError != nullptr; }
    const char* message()     const { return mError ? mError->message : "unknown"; }
    std::string str()         const { return message(); }

    GErrorGuard(const GErrorGuard&) = delete;
    GErrorGuard& operator=(const GErrorGuard&) = delete;
private:
    GError* mError = nullptr;
};

// ── GLib main loop (singleton, background thread) ───────────────────────────

static std::atomic<bool> sGlibLoopStarted{false};

static void ensureGlibMainLoop()
{
    if (sGlibLoopStarted.exchange(true)) return;

    std::thread([]()
    {
        auto* loop = g_main_loop_new(nullptr, FALSE);
        DEBUG(3, "WaylandSession: GLib main loop starting");
        g_main_loop_run(loop);
        g_main_loop_unref(loop);
        DEBUG(3, "WaylandSession: GLib main loop exited");
    }).detach();
}

#endif // HAVE_LIBPORTAL

// ── Singleton ───────────────────────────────────────────────────────────────

WaylandSession& WaylandSession::getInstance()
{
    static WaylandSession instance;
    return instance;
}

// ── Private helpers (now member functions) ───────────────────────────────────

#ifdef HAVE_LIBPORTAL

std::string WaylandSession::buildRequestPath(const std::string& token) const
{
    const char* unique = g_dbus_connection_get_unique_name(_sessionBus);
    std::string sender = unique ? unique : ":0.0";

    // Strip leading ':' and replace '.' with '_' per XDG portal spec
    if (!sender.empty() && sender[0] == ':') sender.erase(0, 1);
    for (char& c : sender)
        if (c == '.') c = '_';

    return "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
}

bool WaylandSession::portalCall(const std::string& interface,
                                const std::string& method,
                                GVariant* params,
                                const GVariantType* replyType,
                                int timeoutMs)
{
    GErrorGuard err;
    UniqueGVariant result(g_dbus_connection_call_sync(
        _sessionBus,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        interface.c_str(),
        method.c_str(),
        params,
        replyType,
        G_DBUS_CALL_FLAGS_NONE,
        timeoutMs,
        nullptr,
        err.out()));

    if (!result)
    {
        ERROR(method + " failed: " + err.str());
        return false;
    }
    return true;
}

bool WaylandSession::createSession()
{
    const std::string token = "coral" + std::to_string(g_random_int());
    const std::string reqPath = buildRequestPath(token);

    // Subscribe to the Response signal before making the call (avoids race)
    guint subId = g_dbus_connection_signal_subscribe(
        _sessionBus,
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        reqPath.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection*, const gchar*, const gchar*, const gchar*,
           const gchar*, GVariant* parameters, gpointer userData)
        {
            guint32 response = 2;
            GVariant* dict = nullptr;
            g_variant_get(parameters, "(u@a{sv})", &response, &dict);
            if (!dict) return;

            GVariant* v = g_variant_lookup_value(dict, "session_handle", G_VARIANT_TYPE_STRING);
            if (v)
            {
                auto* out = static_cast<std::string*>(userData);
                *out = g_variant_get_string(v, nullptr);
                g_variant_unref(v);
            }
            g_variant_unref(dict);
        },
        &_rdpSessionObjectPath,
        nullptr);

    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&opts, "{sv}", "handle_token",
                          g_variant_new_string(token.c_str()));
    g_variant_builder_add(&opts, "{sv}", "session_handle_token",
                          g_variant_new_string(token.c_str()));

    bool ok = portalCall("org.freedesktop.portal.RemoteDesktop",
                         "CreateSession",
                         g_variant_new("(a{sv})", &opts),
                         G_VARIANT_TYPE("(o)"),
                         30000);

    if (!ok)
    {
        g_dbus_connection_signal_unsubscribe(_sessionBus, subId);
        return false;
    }

    // Poll until the signal populates _rdpSessionObjectPath (up to ~2 seconds)
    for (int i = 0; i < 400 && _rdpSessionObjectPath.empty(); ++i)
    {
        while (g_main_context_iteration(nullptr, false)) {}
        g_usleep(5000);
    }
    g_dbus_connection_signal_unsubscribe(_sessionBus, subId);

    if (_rdpSessionObjectPath.empty())
    {
        ERROR("CreateSession did not return a session_handle in time");
        return false;
    }
    return true;
}

bool WaylandSession::selectDevices()
{
    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&opts, "{sv}", "types",        g_variant_new_uint32(1)); // KEYBOARD
    g_variant_builder_add(&opts, "{sv}", "persist_mode", g_variant_new_uint32(1)); // persist while running

    return portalCall("org.freedesktop.portal.RemoteDesktop",
                      "SelectDevices",
                      g_variant_new("(oa{sv})", _rdpSessionObjectPath.c_str(), &opts),
                      G_VARIANT_TYPE("(o)"),
                      30000);
}

bool WaylandSession::startSession()
{
    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);

    return portalCall("org.freedesktop.portal.RemoteDesktop",
                      "Start",
                      g_variant_new("(osa{sv})", _rdpSessionObjectPath.c_str(), "", &opts),
                      G_VARIANT_TYPE("(o)"),
                      60000);
}

bool WaylandSession::notifyKeysym(unsigned int keysym, unsigned int state)
{
    GErrorGuard err;
    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);

    UniqueGVariant result(g_dbus_connection_call_sync(
        _sessionBus,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        "NotifyKeyboardKeysym",
        g_variant_new("(oa{sv}iu)", _rdpSessionObjectPath.c_str(), &opts,
                       static_cast<gint32>(keysym), static_cast<guint32>(state)),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        10000,
        nullptr,
        err.out()));

    if (err)
    {
        ERROR(std::string("NotifyKeyboardKeysym failed: ") + err.str());
        return false;
    }
    return true;
}

#endif // HAVE_LIBPORTAL

// ── Public API ──────────────────────────────────────────────────────────────

bool WaylandSession::ensureInitialized()
{
#ifdef HAVE_LIBPORTAL
    if (_initialized) return true;

    ensureGlibMainLoop();

    _portal = xdp_portal_new();
    if (!_portal)
    {
        ERROR("WaylandSession: failed to create XdpPortal context");
        return false;
    }

    GErrorGuard err;
    _sessionBus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, err.out());
    if (!_sessionBus)
    {
        ERROR("WaylandSession: failed to get session bus: " + err.str());
        return false;
    }

    GErrorGuard rdpErr;
    _remoteDesktopProxy = g_dbus_proxy_new_sync(
        _sessionBus, G_DBUS_PROXY_FLAGS_NONE, nullptr,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        nullptr, rdpErr.out());
    if (!_remoteDesktopProxy)
    {
        ERROR("WaylandSession: failed to create RemoteDesktop proxy: " + rdpErr.str());
        return false;
    }

    GErrorGuard clipErr;
    _clipboardProxy = g_dbus_proxy_new_sync(
        _sessionBus, G_DBUS_PROXY_FLAGS_NONE, nullptr,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Clipboard",
        nullptr, clipErr.out());
    if (!_clipboardProxy)
    {
        DEBUG(3, "WaylandSession: Clipboard portal not available: " + clipErr.str());
    }

    _rdpSessionObjectPath.clear();
    if (!createSession())  return false;
    if (!selectDevices())  return false;
    if (!startSession())   return false;

    _rdpStarted = true;
    _initialized = true;
    DEBUG(3, "WaylandSession: initialized successfully");
    return true;
#else
    DEBUG(3, "WaylandSession: libportal not available at build time");
    return false;
#endif
}

bool WaylandSession::setClipboard(const std::string& text)
{
#ifdef HAVE_LIBPORTAL
    if (!_initialized) return false;
    if (!_clipboardProxy)
    {
        DEBUG(3, "WaylandSession::setClipboard: Clipboard proxy is null");
        return false;
    }

    // Try SetSelection, then SetClipboard (different desktop environments use different methods)
    for (const char* method : {"SetSelection", "SetClipboard"})
    {
        DEBUG(3, std::string("WaylandSession: trying Clipboard method ") + method);
        GErrorGuard err;
        GVariantBuilder opts;
        g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&opts, "{sv}", "text", g_variant_new_string(text.c_str()));

        UniqueGVariant result(g_dbus_proxy_call_sync(
            _clipboardProxy,
            method,
            g_variant_new("(oa{sv})", _rdpSessionObjectPath.c_str(), &opts),
            G_DBUS_CALL_FLAGS_NONE,
            10000,
            nullptr,
            err.out()));

        if (result)
        {
            DEBUG(3, std::string("WaylandSession: Clipboard set via ") + method);
            return true;
        }
        DEBUG(3, std::string("WaylandSession::") + method + " failed: " + err.str());
    }

    DEBUG(3, "WaylandSession: Clipboard portal calls failed; will fall back to typing");
    return false;
#else
    return false;
#endif
}

bool WaylandSession::sendCtrlV()
{
#ifdef HAVE_LIBPORTAL
    if (!_initialized || !_rdpStarted || _rdpSessionObjectPath.empty()) return false;

    constexpr unsigned int XK_Control_L = 0xffe3;
    constexpr unsigned int XK_v         = 0x0076;

    bool ok = true;
    ok &= notifyKeysym(XK_Control_L, 1);
    ok &= notifyKeysym(XK_v, 1);
    ok &= notifyKeysym(XK_v, 0);
    ok &= notifyKeysym(XK_Control_L, 0);
    return ok;
#else
    return false;
#endif
}

bool WaylandSession::typeText(const std::string& text)
{
#ifdef HAVE_LIBPORTAL
    if (!_initialized || !_rdpStarted || _rdpSessionObjectPath.empty()) return false;

    // Map ASCII char → (X11 keysym, needsShift)
    auto charToKeysym = [](char c) -> std::pair<unsigned int, bool>
    {
        if (c >= 'a' && c <= 'z') return {static_cast<unsigned>(c), false};
        if (c >= 'A' && c <= 'Z') return {static_cast<unsigned>(c + 32), true};
        if (c >= '0' && c <= '9') return {static_cast<unsigned>(c), false};

        // clang-format off
        switch (c)
        {
            case ' ':  return {0x0020, false};
            case '\n': return {0xff0d, false};
            case '\t': return {0xff09, false};
            case '.':  return {0x002e, false};
            case ',':  return {0x002c, false};
            case '-':  return {0x002d, false};
            case '_':  return {0x002d, true};
            case '+':  return {0x003d, true};
            case '=':  return {0x003d, false};
            case '/':  return {0x002f, false};
            case ':':  return {0x003b, true};
            case ';':  return {0x003b, false};
            case '\'': return {0x0027, false};
            case '"':  return {0x0027, true};
            case '[':  return {0x005b, false};
            case ']':  return {0x005d, false};
            case '(':  return {0x0039, true};
            case ')':  return {0x0030, true};
            case '!':  return {0x0031, true};
            case '?':  return {0x002f, true};
            default:   return {0, false};        // unsupported — skip
        }
        // clang-format on
    };

    constexpr unsigned int XK_Shift_L = 0xffe1;
    bool allOk = true;

    for (char c : text)
    {
        auto [ks, shift] = charToKeysym(c);
        if (ks == 0) continue;  // unsupported character

        bool ok = true;
        if (shift) ok &= notifyKeysym(XK_Shift_L, 1);
        ok &= notifyKeysym(ks, 1);
        ok &= notifyKeysym(ks, 0);
        if (shift) ok &= notifyKeysym(XK_Shift_L, 0);
        if (!ok) allOk = false;
    }
    return allOk;
#else
    return false;
#endif
}
