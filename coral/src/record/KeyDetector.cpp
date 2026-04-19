#include "KeyDetector.h"
#include <algorithm>
#include <cctype>

static std::string toLowerStr(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return out;
}

#if defined(_WIN32)
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <sstream>
#endif

// ── Helpers (Linux) ─────────────────────────────────────────────────────────
#if !defined(_WIN32)

// Split string by delimiter
static std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        tokens.push_back(token);
    }
    return tokens;
}

// ── evdev key name → Linux KEY_* code ───────────────────────────────────────
static int keyNameToEvdevCode(const std::string& name)
{
    static const std::unordered_map<std::string, int> map = {
        // Modifiers (lowercase for case-insensitive lookup)
        {"ctrl",  KEY_LEFTCTRL},  {"control", KEY_LEFTCTRL},
        {"alt",   KEY_LEFTALT},   {"shift",   KEY_LEFTSHIFT},
        {"super", KEY_LEFTMETA},  {"meta",    KEY_LEFTMETA},
        // Function keys
        {"f1", KEY_F1}, {"f2", KEY_F2}, {"f3", KEY_F3}, {"f4", KEY_F4},
        {"f5", KEY_F5}, {"f6", KEY_F6}, {"f7", KEY_F7}, {"f8", KEY_F8},
        {"f9", KEY_F9}, {"f10", KEY_F10}, {"f11", KEY_F11}, {"f12", KEY_F12},
        // Special keys
        {"space",    KEY_SPACE},
        {"num_lock", KEY_NUMLOCK},
        {"tab",      KEY_TAB},
        {"escape",   KEY_ESC},
        {"return",   KEY_ENTER},
        {"enter",    KEY_ENTER},
        // Letters a-z
        {"a", KEY_A}, {"b", KEY_B}, {"c", KEY_C}, {"d", KEY_D},
        {"e", KEY_E}, {"f", KEY_F}, {"g", KEY_G}, {"h", KEY_H},
        {"i", KEY_I}, {"j", KEY_J}, {"k", KEY_K}, {"l", KEY_L},
        {"m", KEY_M}, {"n", KEY_N}, {"o", KEY_O}, {"p", KEY_P},
        {"q", KEY_Q}, {"r", KEY_R}, {"s", KEY_S}, {"t", KEY_T},
        {"u", KEY_U}, {"v", KEY_V}, {"w", KEY_W}, {"x", KEY_X},
        {"y", KEY_Y}, {"z", KEY_Z},
        // Digits 0-9
        {"0", KEY_0}, {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3},
        {"4", KEY_4}, {"5", KEY_5}, {"6", KEY_6}, {"7", KEY_7},
        {"8", KEY_8}, {"9", KEY_9},
    };
    auto it = map.find(toLowerStr(name));
    if (it != map.end()) return it->second;
    return -1;
}

// ── X11 key name → KeySym (kept for fallback) ──────────────────────────────
static KeySym keyNameToKeySym(const std::string& keyName)
{
    static const std::unordered_map<std::string, KeySym> keyMap = {
        {"f1", XK_F1}, {"f2", XK_F2}, {"f3", XK_F3}, {"f4", XK_F4},
        {"f5", XK_F5}, {"f6", XK_F6}, {"f7", XK_F7}, {"f8", XK_F8},
        {"f9", XK_F9}, {"f10", XK_F10}, {"f11", XK_F11}, {"f12", XK_F12},
        {"ctrl", XK_Control_L}, {"alt", XK_Alt_L}, {"shift", XK_Shift_L},
        {"super", XK_Super_L}, {"space", XK_space}, {"num_lock", XK_Num_Lock},
    };
    auto it = keyMap.find(toLowerStr(keyName));
    if (it != keyMap.end()) return it->second;
    if (keyName.length() == 1) return XStringToKeysym(keyName.c_str());
    return NoSymbol;
}

#endif // !_WIN32

// ── Constructor / Destructor ────────────────────────────────────────────────

KeyDetector::KeyDetector()
{
#if !defined(_WIN32)
    openEvdevKeyboards();
#endif
}

KeyDetector::~KeyDetector()
{
#if !defined(_WIN32)
    for (int fd : mEvdevFds)
    {
        close(fd);
    }
    mEvdevFds.clear();
#endif
}

// ── evdev device discovery ──────────────────────────────────────────────────
#if !defined(_WIN32)

void KeyDetector::openEvdevKeyboards()
{
    DIR* dir = opendir("/dev/input");
    if (!dir)
    {
        std::cerr << "[KeyDetector] Cannot open /dev/input — evdev unavailable.\n";
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // Only look at event* files
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        std::string devPath = std::string("/dev/input/") + entry->d_name;
        int fd = open(devPath.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;  // permission denied or other error

        // Check if this device has EV_KEY capability (i.e. it's a keyboard)
        unsigned long evBits = 0;
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evBits)), &evBits) < 0)
        {
            close(fd);
            continue;
        }
        if (!(evBits & (1UL << EV_KEY)))
        {
            close(fd);
            continue;
        }

        // Check if it has actual keyboard keys (e.g. KEY_A = 30)
        unsigned long keyBits[KEY_MAX / (sizeof(unsigned long) * 8) + 1] = {};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0)
        {
            close(fd);
            continue;
        }
        // Verify it has at least KEY_A (filter out mice, joysticks, etc.)
        bool hasKeyboardKeys = (keyBits[KEY_A / (sizeof(unsigned long) * 8)] >>
                                (KEY_A % (sizeof(unsigned long) * 8))) & 1;
        if (!hasKeyboardKeys)
        {
            close(fd);
            continue;
        }

        mEvdevFds.push_back(fd);
    }
    closedir(dir);

    if (!mEvdevFds.empty())
    {
        mUseEvdev = true;
        std::cerr << "[KeyDetector] Using evdev with " << mEvdevFds.size()
                  << " keyboard device(s) — works on X11 and Wayland.\n";
    }
    else
    {
        std::cerr << "[KeyDetector] No evdev keyboard devices accessible.\n";
        std::cerr << "[KeyDetector] Add your user to the 'input' group: sudo usermod -aG input $USER\n";
    }
}

// ── evdev key state query ───────────────────────────────────────────────────

bool KeyDetector::isKeyPressedEvdev(const std::string& keyCombo)
{
    std::vector<std::string> keys = split(keyCombo, '+');

    // Query key state from all keyboard devices.
    // EVIOCGKEY returns a bitmap of currently pressed keys.
    // Combine state from all devices (external + built-in keyboards).
    unsigned char combinedState[KEY_MAX / 8 + 1] = {};

    for (int fd : mEvdevFds)
    {
        unsigned char state[KEY_MAX / 8 + 1] = {};
        if (ioctl(fd, EVIOCGKEY(sizeof(state)), state) >= 0)
        {
            for (size_t i = 0; i < sizeof(state); ++i)
            {
                combinedState[i] |= state[i];
            }
        }
    }

    // Check every key in the combo is pressed.
    // For modifiers (Ctrl, Alt, Shift, Super), check both left and right variants.
    for (const auto& key : keys)
    {
        int code = keyNameToEvdevCode(key);
        if (code < 0)
        {
            std::cerr << "[KeyDetector] Unknown key name: '" << key << "'.\n";
            return false;
        }

        bool pressed = (combinedState[code / 8] >> (code % 8)) & 1;

        // Also check right-side modifier variants
        if (!pressed)
        {
            int altCode = -1;
            if (code == KEY_LEFTCTRL)  altCode = KEY_RIGHTCTRL;
            if (code == KEY_LEFTALT)   altCode = KEY_RIGHTALT;
            if (code == KEY_LEFTSHIFT) altCode = KEY_RIGHTSHIFT;
            if (code == KEY_LEFTMETA)  altCode = KEY_RIGHTMETA;
            if (altCode >= 0)
            {
                pressed = (combinedState[altCode / 8] >> (altCode % 8)) & 1;
            }
        }

        if (!pressed) return false;
    }
    return true;
}

// ── X11 fallback ────────────────────────────────────────────────────────────

bool KeyDetector::isKeyPressedX11(const std::string& keyCombo)
{
    if (keyCombo.find("Fn") != std::string::npos)
    {
        std::cerr << "[KeyDetector] 'Fn' key cannot be detected in software. "
                     "Please use another key in config.json.\n";
        return false;
    }
    Display* display = XOpenDisplay(nullptr);
    if (!display)
    {
        std::cerr << "[KeyDetector] Cannot open X display.\n";
        return false;
    }
    std::vector<std::string> keys = split(keyCombo, '+');
    char keymap[32];
    XQueryKeymap(display, keymap);
    bool allPressed = true;
    for (const auto& key : keys)
    {
        KeySym keysym = keyNameToKeySym(key);
        if (keysym == NoSymbol)
        {
            std::cerr << "[KeyDetector] Unknown key name: '" << key << "'.\n";
            allPressed = false;
            break;
        }
        KeyCode keycode = XKeysymToKeycode(display, keysym);
        if (!(keymap[keycode / 8] & (1 << (keycode % 8))))
        {
            allPressed = false;
            break;
        }
    }
    XCloseDisplay(display);
    return allPressed;
}

#endif // !_WIN32

// ── Public API ──────────────────────────────────────────────────────────────

bool KeyDetector::isTriggerKeyPressed(const std::string& keyCombo)
{
#if defined(_WIN32)
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    std::string lc = toLowerStr(keyCombo);
    bool needCtrl = lc.find("ctrl") != std::string::npos || lc.find("control") != std::string::npos;
    bool needShift = lc.find("shift") != std::string::npos;
    bool needAlt = lc.find("alt") != std::string::npos;

    // Modifier-only trigger (e.g. "Ctrl", "Alt", "Shift"): pressed when that modifier is held
    bool modifierOnly = (needCtrl && !needShift && !needAlt) || (!needCtrl && needShift && !needAlt) || (!needCtrl && !needShift && needAlt);
    if (modifierOnly && keyCombo.find('+') == std::string::npos)
    {
        if (needCtrl) return ctrl;
        if (needShift) return shift;
        if (needAlt) return alt;
    }

    size_t pos = keyCombo.rfind('+');
    std::string primary = (pos == std::string::npos) ? keyCombo : keyCombo.substr(pos + 1);
    primary.erase(std::remove_if(primary.begin(), primary.end(), ::isspace), primary.end());
    std::string primaryLc = toLowerStr(primary);
    SHORT vkey = 0;
    if (primary.size() == 1)
    {
        char c = primary[0];
        if (c >= 'A' && c <= 'Z') vkey = c;
        else if (c >= 'a' && c <= 'z') vkey = toupper(c);
        else if (c >= '0' && c <= '9') vkey = c;
    }
    else if (primaryLc == "f1") vkey = VK_F1;
    else if (primaryLc == "f2") vkey = VK_F2;

    bool primaryDown = vkey ? ((GetAsyncKeyState(vkey) & 0x8000) != 0) : false;
    return (!needCtrl || ctrl) && (!needShift || shift) && (!needAlt || alt) && primaryDown;
#else
    // Prefer evdev (works on both X11 and Wayland)
    if (mUseEvdev)
    {
        return isKeyPressedEvdev(keyCombo);
    }
    // Fall back to X11
    return isKeyPressedX11(keyCombo);
#endif
}
