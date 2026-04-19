#include "ClipboardOwner.h"
#include "Logger.h"
#include <cstring>
#include <thread>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#else
#include <X11/Xatom.h>
#endif

#if defined(_WIN32)
ClipboardOwner::ClipboardOwner() {}
#else
ClipboardOwner::ClipboardOwner(Display* display, Window ownerWindow)
    : _display(display), _window(ownerWindow)
{
    _clipboardAtom = XInternAtom(_display, "CLIPBOARD", False);
    _primaryAtom = XInternAtom(_display, "PRIMARY", False);
    _utf8Atom = XInternAtom(_display, "UTF8_STRING", False);
    _targetsAtom = XInternAtom(_display, "TARGETS", False);
    _textAtom = XInternAtom(_display, "TEXT", False);
    _compoundTextAtom = XInternAtom(_display, "COMPOUND_TEXT", False);
    _textPlainAtom = XInternAtom(_display, "text/plain", False);
    _textPlainUtf8Atom = XInternAtom(_display, "text/plain;charset=utf-8", False);

    XSetSelectionOwner(_display, _clipboardAtom, _window, CurrentTime);
    XSetSelectionOwner(_display, _primaryAtom, _window, CurrentTime);
    XFlush(_display);
    XSync(_display, False);
}
#endif

void ClipboardOwner::serveRequests(const std::string& text,
                                   int timeoutMs,
                                   int pollIntervalMs,
                                   int idleExitMs)
{
#if defined(_WIN32)
    // Windows: Set clipboard content once and wait briefly to simulate service window
    auto start = std::chrono::steady_clock::now();
    int servedRequests = 0;

    if (OpenClipboard(NULL))
    {
        EmptyClipboard();

        const size_t bytesNeeded = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, bytesNeeded);
        if (hglbCopy)
        {
            wchar_t* target = static_cast<wchar_t*>(GlobalLock(hglbCopy));
            if (target)
            {
                int written = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, target, (int)(bytesNeeded / sizeof(wchar_t)));
                (void)written;
                GlobalUnlock(hglbCopy);
                SetClipboardData(CF_UNICODETEXT, hglbCopy);
                servedRequests++;
            }
            else
            {
                GlobalFree(hglbCopy);
            }
        }

        CloseClipboard();
    }

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeoutMs))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
    }

    if (servedRequests == 0)
    {
        DEBUG(3, "No clipboard requests were served during timeout period (Windows)");
    }
    else
    {
        DEBUG(3, "Clipboard text set (Windows)");
    }
#else
    XEvent event;
    auto start = std::chrono::steady_clock::now();
    auto lastActivity = start;
    int servedRequests = 0;
    bool hasServedUTF8 = false;  // Track if we've already served UTF8 to prevent duplicates

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeoutMs))
    {
        while (XCheckTypedWindowEvent(_display, _window, SelectionRequest, &event))
        {
            XSelectionRequestEvent* req = &event.xselectionrequest;

            XEvent respond;
            memset(&respond, 0, sizeof(respond));
            respond.xselection.type = SelectionNotify;
            respond.xselection.display = req->display;
            respond.xselection.requestor = req->requestor;
            respond.xselection.selection = req->selection;
            respond.xselection.target = req->target;
            respond.xselection.time = req->time;
            respond.xselection.property = req->property;

            if (req->selection != _clipboardAtom && req->selection != _primaryAtom)
            {
                respond.xselection.property = None;
                XSendEvent(_display, req->requestor, 0, 0, &respond);
                XFlush(_display);
                continue;
            }

            //if (req->target == _utf8Atom && !hasServedUTF8)
            if (req->target == _utf8Atom)
            {
                DEBUG(3, "Setting clipboard with UTF8_STRING target and text is " + text);
                XChangeProperty(_display, req->requestor, req->property, _utf8Atom, 8,
                                PropModeReplace, (const unsigned char*)text.c_str(), text.size());
                servedRequests++;
                lastActivity = std::chrono::steady_clock::now();
                hasServedUTF8 = true;  // Mark that we've served UTF8 to prevent duplicates
            }
            else if (req->target == XA_STRING)
            {
                XChangeProperty(_display, req->requestor, req->property, XA_STRING, 8,
                                PropModeReplace, (const unsigned char*)text.c_str(), text.size());
                servedRequests++;
                lastActivity = std::chrono::steady_clock::now();
            }
            else if (req->target == _textAtom)
            {
                XChangeProperty(_display, req->requestor, req->property, _textAtom, 8,
                                PropModeReplace, (const unsigned char*)text.c_str(), text.size());
                servedRequests++;
                lastActivity = std::chrono::steady_clock::now();
            }
            else if (req->target == _compoundTextAtom)
            {
                XChangeProperty(_display, req->requestor, req->property, _compoundTextAtom, 8,
                                PropModeReplace, (const unsigned char*)text.c_str(), text.size());
                servedRequests++;
                lastActivity = std::chrono::steady_clock::now();
            }
            else if (req->target == _textPlainAtom)
            {
                XChangeProperty(_display, req->requestor, req->property, _textPlainAtom, 8,
                                PropModeReplace, (const unsigned char*)text.c_str(), text.size());
                servedRequests++;
                lastActivity = std::chrono::steady_clock::now();
            }
            else if (req->target == _textPlainUtf8Atom)
            {
                XChangeProperty(_display, req->requestor, req->property, _textPlainUtf8Atom, 8,
                                PropModeReplace, (const unsigned char*)text.c_str(), text.size());
                servedRequests++;
                lastActivity = std::chrono::steady_clock::now();
            }
            else if (req->target == _targetsAtom)
            {
                Atom targets[] = { _utf8Atom, XA_STRING, _textAtom, _compoundTextAtom, _textPlainAtom, _textPlainUtf8Atom };
                XChangeProperty(_display, req->requestor, req->property, XA_ATOM, 32,
                                PropModeReplace, (unsigned char*)targets, sizeof(targets) / sizeof(targets[0]));
                lastActivity = std::chrono::steady_clock::now();
            }
            else
            {
                char* target_name = XGetAtomName(_display, req->target);
                DEBUG(3, std::string("Unsupported target type: ") + (target_name ? target_name : "UNKNOWN"));
                if (target_name) XFree(target_name);

                XSendEvent(_display, req->requestor, 0, 0, &respond);
                XFlush(_display);
                lastActivity = std::chrono::steady_clock::now();
            }
        }

        // Exit early if we've served requests and been idle
        //if (servedRequests > 0 && std::chrono::steady_clock::now() - lastActivity > std::chrono::milliseconds(idleExitMs))
        if (std::chrono::steady_clock::now() - lastActivity > std::chrono::milliseconds(idleExitMs))
        {
            DEBUG(3, "Exiting clipboard serving after " + std::to_string(servedRequests) + " requests served");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
    }
    
    if (servedRequests == 0) 
    {
        DEBUG(3, "No clipboard requests were served during timeout period");
    } 
    else 
    {
        DEBUG(3, "Served " + std::to_string(servedRequests) + " clipboard requests");
    }
#endif
}