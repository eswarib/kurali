#ifndef CLIPBOARD_OWNER_H
#define CLIPBOARD_OWNER_H

#if defined(_WIN32)
#include <string>
#else
#include <X11/Xlib.h>
#include <string>
#endif

class ClipboardOwner {
public:
    #if defined(_WIN32)
    ClipboardOwner();
    #else
    ClipboardOwner(Display* display, Window ownerWindow);
    #endif

    void serveRequests(const std::string& text,
                       int timeoutMs,
                       int pollIntervalMs,
                       int idleExitMs);

private:
    #if !defined(_WIN32)
    Display* _display;
    Window _window;
    Atom _clipboardAtom;
    Atom _primaryAtom;
    Atom _utf8Atom;
    Atom _targetsAtom;
    Atom _textAtom;                 // "TEXT"
    Atom _compoundTextAtom;         // "COMPOUND_TEXT"
    Atom _textPlainAtom;            // "text/plain"
    Atom _textPlainUtf8Atom;        // "text/plain;charset=utf-8"
    #endif
};

#endif // CLIPBOARD_OWNER_H

