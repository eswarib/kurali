#include "A11yInjector.h"
#include "Logger.h"

#ifdef HAVE_ATSPI
#include <atspi/atspi.h>
#endif

A11yInjector& A11yInjector::getInstance()
{
    static A11yInjector instance;
    return instance;
}

bool A11yInjector::typeText(const std::string& text)
{
#ifdef HAVE_ATSPI
    GError* error = nullptr;
    if (!atspi_init())
    {
        DEBUG(3, "A11yInjector: atspi_init failed");
        return false;
    }

    AtspiAccessible* desktop = atspi_get_desktop(0);
    if (!desktop)
    {
        DEBUG(3, "A11yInjector: no desktop accessible");
        return false;
    }

    // Walk the desktop tree to find the focused accessible.
    // atspi_get_focus() does not exist in AT-SPI2; instead we iterate
    // over applications and their children looking for ATSPI_STATE_FOCUSED.
    AtspiAccessible* focus = nullptr;
    int nApps = atspi_accessible_get_child_count(desktop, &error);
    if (error) { g_error_free(error); error = nullptr; }
    for (int i = 0; i < nApps && !focus; ++i)
    {
        AtspiAccessible* app = atspi_accessible_get_child_at_index(desktop, i, &error);
        if (error) { g_error_free(error); error = nullptr; }
        if (!app) continue;
        int nChildren = atspi_accessible_get_child_count(app, &error);
        if (error) { g_error_free(error); error = nullptr; }
        for (int j = 0; j < nChildren && !focus; ++j)
        {
            AtspiAccessible* child = atspi_accessible_get_child_at_index(app, j, &error);
            if (error) { g_error_free(error); error = nullptr; }
            if (!child) continue;
            AtspiStateSet* states = atspi_accessible_get_state_set(child);
            if (states && atspi_state_set_contains(states, ATSPI_STATE_FOCUSED))
            {
                focus = child;  // found it, don't unref
            }
            else
            {
                g_object_unref(child);
            }
            if (states) g_object_unref(states);
        }
        g_object_unref(app);
    }
    if (!focus)
    {
        DEBUG(3, "A11yInjector: no focused accessible found");
        g_object_unref(desktop);
        return false;
    }

    // Try EditableText first
    AtspiEditableText* editable = atspi_accessible_get_editable_text_iface(focus);
    if (editable)
    {
        bool ok = false;
        // Replace contents if possible; fall back to insert
        if (atspi_editable_text_set_text_contents(editable, text.c_str(), &error))
        {
            ok = true;
        }
        else
        {
            if (error) { g_error_free(error); error = nullptr; }
            // Insert at caret position 0 as a fallback
            if (atspi_editable_text_insert_text(editable, 0, text.c_str(), text.size(), &error))
            {
                ok = true;
            }
            else
            {
                DEBUG(3, std::string("A11yInjector: insert_text failed: ") + (error ? error->message : "unknown"));
            }
        }
        g_object_unref(focus);
        g_object_unref(desktop);
        return ok;
    }

    g_object_unref(focus);
    g_object_unref(desktop);
    return false;
#else
    return false;
#endif
} 