#ifndef UINPUT_INJECTOR_H
#define UINPUT_INJECTOR_H

#include <string>

#if !defined(_WIN32)

// UInputInjector: injects text by writing key events to /dev/uinput.
// Works on both X11 and Wayland — requires the user to be in the 'input' group.
// This is the same mechanism used by ydotool.
class UInputInjector
{
public:
    static UInputInjector& getInstance();
    ~UInputInjector();

    // Returns true if /dev/uinput is accessible and the virtual device was created
    bool isAvailable() const { return mFd >= 0; }

    // Type a string by simulating key press/release events.
    // Returns true if all characters were injected successfully.
    bool typeText(const std::string& text);

private:
    UInputInjector();
    UInputInjector(const UInputInjector&) = delete;
    UInputInjector& operator=(const UInputInjector&) = delete;

    int mFd = -1;  // /dev/uinput file descriptor

    bool createVirtualDevice();
    void destroyVirtualDevice();

    // Send a single key event (press or release)
    bool sendKeyEvent(int keyCode, bool pressed);

    // Emit SYN_REPORT to flush the event
    bool sendSync();

    // Type a single character (press + release, with shift if needed)
    bool typeChar(char c);
};

#endif // !_WIN32
#endif // UINPUT_INJECTOR_H
