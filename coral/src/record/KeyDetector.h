#ifndef KEY_DETECTOR_H
#define KEY_DETECTOR_H

#include <string>
#include <vector>

// KeyDetector: handles detection of trigger key press.
// On Linux, prefers evdev (works on both X11 and Wayland).
// Falls back to X11 XQueryKeymap if evdev devices cannot be opened.
class KeyDetector
{
public:
    KeyDetector();
    virtual ~KeyDetector();
    // Returns true if the trigger key combo is currently pressed
    virtual bool isTriggerKeyPressed(const std::string& keyCombo);

#if !defined(_WIN32)
    // Returns true if evdev was successfully initialized
    bool hasEvdevAccess() const { return mUseEvdev; }
private:
    std::vector<int> mEvdevFds;   // open file descriptors for /dev/input/event*
    bool mUseEvdev = false;

    void openEvdevKeyboards();
    bool isKeyPressedEvdev(const std::string& keyCombo);
    bool isKeyPressedX11(const std::string& keyCombo);
#endif
};

#endif // KEY_DETECTOR_H