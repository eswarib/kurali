#include "UInputInjector.h"

#if !defined(_WIN32)

#include "Logger.h"
#include <linux/uinput.h>
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <chrono>

// Tunables (uinput is key-by-key; we batch SYN_REPORT per character to cut syscalls.)
//
// kInterCharacterDelay: pause after each character’s full key sequence. If the focused
// app drops or reorders keys (VM, remote desktop, slow editor), increase this (try
// 3000–8000 µs). Decrease only after testing. Monitor throughput with debugLevel >= 3
// in ~/.coral/conf/config.json ("debugLevel": 3) — see coral.log for lines like:
//   UInputInjector: typed 1000 chars in 2500 ms (~2500 µs/char)
namespace {
constexpr std::chrono::microseconds kInterCharacterDelay{1500};
} // namespace

// ── Character → (keycode, needsShift) mapping ──────────────────────────────

struct KeyMapping
{
    int code;
    bool shift;
};

static const std::unordered_map<char, KeyMapping>& charMap()
{
    static const std::unordered_map<char, KeyMapping> map = {
        // Lowercase letters
        {'a', {KEY_A, false}}, {'b', {KEY_B, false}}, {'c', {KEY_C, false}},
        {'d', {KEY_D, false}}, {'e', {KEY_E, false}}, {'f', {KEY_F, false}},
        {'g', {KEY_G, false}}, {'h', {KEY_H, false}}, {'i', {KEY_I, false}},
        {'j', {KEY_J, false}}, {'k', {KEY_K, false}}, {'l', {KEY_L, false}},
        {'m', {KEY_M, false}}, {'n', {KEY_N, false}}, {'o', {KEY_O, false}},
        {'p', {KEY_P, false}}, {'q', {KEY_Q, false}}, {'r', {KEY_R, false}},
        {'s', {KEY_S, false}}, {'t', {KEY_T, false}}, {'u', {KEY_U, false}},
        {'v', {KEY_V, false}}, {'w', {KEY_W, false}}, {'x', {KEY_X, false}},
        {'y', {KEY_Y, false}}, {'z', {KEY_Z, false}},
        // Uppercase letters (same key + shift)
        {'A', {KEY_A, true}}, {'B', {KEY_B, true}}, {'C', {KEY_C, true}},
        {'D', {KEY_D, true}}, {'E', {KEY_E, true}}, {'F', {KEY_F, true}},
        {'G', {KEY_G, true}}, {'H', {KEY_H, true}}, {'I', {KEY_I, true}},
        {'J', {KEY_J, true}}, {'K', {KEY_K, true}}, {'L', {KEY_L, true}},
        {'M', {KEY_M, true}}, {'N', {KEY_N, true}}, {'O', {KEY_O, true}},
        {'P', {KEY_P, true}}, {'Q', {KEY_Q, true}}, {'R', {KEY_R, true}},
        {'S', {KEY_S, true}}, {'T', {KEY_T, true}}, {'U', {KEY_U, true}},
        {'V', {KEY_V, true}}, {'W', {KEY_W, true}}, {'X', {KEY_X, true}},
        {'Y', {KEY_Y, true}}, {'Z', {KEY_Z, true}},
        // Digits
        {'0', {KEY_0, false}}, {'1', {KEY_1, false}}, {'2', {KEY_2, false}},
        {'3', {KEY_3, false}}, {'4', {KEY_4, false}}, {'5', {KEY_5, false}},
        {'6', {KEY_6, false}}, {'7', {KEY_7, false}}, {'8', {KEY_8, false}},
        {'9', {KEY_9, false}},
        // Whitespace
        {' ',  {KEY_SPACE, false}},
        {'\n', {KEY_ENTER, false}},
        {'\t', {KEY_TAB,   false}},
        // Punctuation (US keyboard layout)
        {'.',  {KEY_DOT,        false}},
        {',',  {KEY_COMMA,      false}},
        {'-',  {KEY_MINUS,      false}},
        {'=',  {KEY_EQUAL,      false}},
        {';',  {KEY_SEMICOLON,  false}},
        {'\'', {KEY_APOSTROPHE, false}},
        {'/',  {KEY_SLASH,      false}},
        {'\\', {KEY_BACKSLASH,  false}},
        {'[',  {KEY_LEFTBRACE,  false}},
        {']',  {KEY_RIGHTBRACE, false}},
        {'`',  {KEY_GRAVE,      false}},
        // Shifted punctuation (US layout)
        {'!',  {KEY_1,          true}},
        {'@',  {KEY_2,          true}},
        {'#',  {KEY_3,          true}},
        {'$',  {KEY_4,          true}},
        {'%',  {KEY_5,          true}},
        {'^',  {KEY_6,          true}},
        {'&',  {KEY_7,          true}},
        {'*',  {KEY_8,          true}},
        {'(',  {KEY_9,          true}},
        {')',  {KEY_0,          true}},
        {'_',  {KEY_MINUS,      true}},
        {'+',  {KEY_EQUAL,      true}},
        {'{',  {KEY_LEFTBRACE,  true}},
        {'}',  {KEY_RIGHTBRACE, true}},
        {':',  {KEY_SEMICOLON,  true}},
        {'"',  {KEY_APOSTROPHE, true}},
        {'<',  {KEY_COMMA,      true}},
        {'>',  {KEY_DOT,        true}},
        {'?',  {KEY_SLASH,      true}},
        {'|',  {KEY_BACKSLASH,  true}},
        {'~',  {KEY_GRAVE,      true}},
    };
    return map;
}

// All key codes we need to register with the virtual device
static const int ALL_KEY_CODES[] = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_SPACE, KEY_ENTER, KEY_TAB,
    KEY_DOT, KEY_COMMA, KEY_MINUS, KEY_EQUAL, KEY_SEMICOLON, KEY_APOSTROPHE,
    KEY_SLASH, KEY_BACKSLASH, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_GRAVE,
    KEY_LEFTSHIFT,
};

// ── Singleton ───────────────────────────────────────────────────────────────

UInputInjector& UInputInjector::getInstance()
{
    static UInputInjector instance;
    return instance;
}

UInputInjector::UInputInjector()
{
    createVirtualDevice();
}

UInputInjector::~UInputInjector()
{
    destroyVirtualDevice();
}

// ── Virtual device lifecycle ────────────────────────────────────────────────

bool UInputInjector::createVirtualDevice()
{
    mFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (mFd < 0)
    {
        DEBUG(3, "UInputInjector: cannot open /dev/uinput — not available");
        return false;
    }

    // Enable EV_KEY event type
    if (ioctl(mFd, UI_SET_EVBIT, EV_KEY) < 0)
    {
        ERROR("UInputInjector: failed to set EV_KEY");
        close(mFd);
        mFd = -1;
        return false;
    }

    // Register all key codes we'll use
    for (int code : ALL_KEY_CODES)
    {
        if (ioctl(mFd, UI_SET_KEYBIT, code) < 0)
        {
            ERROR("UInputInjector: failed to set key bit for code " + std::to_string(code));
            close(mFd);
            mFd = -1;
            return false;
        }
    }

    // Set up the virtual device description
    struct uinput_setup setup{};
    std::strncpy(setup.name, "coral-keyboard", UINPUT_MAX_NAME_SIZE - 1);
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor  = 0x1234;
    setup.id.product = 0x5678;
    setup.id.version = 1;

    if (ioctl(mFd, UI_DEV_SETUP, &setup) < 0)
    {
        ERROR("UInputInjector: UI_DEV_SETUP failed");
        close(mFd);
        mFd = -1;
        return false;
    }

    if (ioctl(mFd, UI_DEV_CREATE) < 0)
    {
        ERROR("UInputInjector: UI_DEV_CREATE failed");
        close(mFd);
        mFd = -1;
        return false;
    }

    // Small delay to let the kernel register the new device
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    INFO("UInputInjector: virtual keyboard device created — works on X11 and Wayland");
    return true;
}

void UInputInjector::destroyVirtualDevice()
{
    if (mFd >= 0)
    {
        ioctl(mFd, UI_DEV_DESTROY);
        close(mFd);
        mFd = -1;
    }
}

// ── Low-level event writing ─────────────────────────────────────────────────

bool UInputInjector::sendKeyEvent(int keyCode, bool pressed)
{
    struct input_event ev{};
    ev.type  = EV_KEY;
    ev.code  = static_cast<__u16>(keyCode);
    ev.value = pressed ? 1 : 0;

    return write(mFd, &ev, sizeof(ev)) == sizeof(ev);
}

bool UInputInjector::sendSync()
{
    struct input_event ev{};
    ev.type  = EV_SYN;
    ev.code  = SYN_REPORT;
    ev.value = 0;

    return write(mFd, &ev, sizeof(ev)) == sizeof(ev);
}

// ── Character typing ────────────────────────────────────────────────────────

bool UInputInjector::typeChar(char c)
{
    const auto& map = charMap();
    auto it = map.find(c);
    if (it == map.end())
    {
        DEBUG(3, std::string("UInputInjector: unsupported character '") + c + "', skipping");
        return true;  // skip unknown chars, don't fail the whole string
    }

    const auto& [code, shift] = it->second;
    bool ok = true;

    // One SYN_REPORT after the full press/release sequence for this character (fewer
    // writes than syncing after each EV_KEY; same semantics for userspace input stack).
    if (shift)
    {
        ok &= sendKeyEvent(KEY_LEFTSHIFT, true);
    }
    ok &= sendKeyEvent(code, true);
    ok &= sendKeyEvent(code, false);
    if (shift)
    {
        ok &= sendKeyEvent(KEY_LEFTSHIFT, false);
    }
    ok &= sendSync();

    std::this_thread::sleep_for(kInterCharacterDelay);

    return ok;
}

// ── Public API ──────────────────────────────────────────────────────────────

bool UInputInjector::typeText(const std::string& text)
{
    if (mFd < 0)
    {
        DEBUG(3, "UInputInjector: virtual device not available");
        return false;
    }

    const auto t0 = std::chrono::steady_clock::now();
    bool allOk = true;
    size_t typed = 0;
    for (char c : text)
    {
        if (!typeChar(c)) allOk = false;
        ++typed;
    }
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    const long long msTotal = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    const long long usTotal = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    if (typed >= 100)
    {
        const long long usPerChar = typed ? (usTotal / static_cast<long long>(typed)) : 0;
        DEBUG(1, "UInputInjector: typed " + std::to_string(typed) + " chars in " +
                     std::to_string(msTotal) + " ms (~" + std::to_string(usPerChar) +
                     " µs/char); if keys drop, raise kInterCharacterDelay in UInputInjector.cpp");
    }

    return allOk;
}

#endif // !_WIN32
