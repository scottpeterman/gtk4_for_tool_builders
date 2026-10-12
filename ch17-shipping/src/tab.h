// Tab: everything one notebook page owns. The page owns its Tab through
// g_object_set_data_full, so removing the page deletes the Tab, and with it
// the pty session.
#pragma once

#include <gtk/gtk.h>

#include <memory>

#include "core/session.h"
#include "pty_session.h"
#include "session_log.h"

struct Tab {
    Session session;
    GtkWidget *page = nullptr;   // not owned: the notebook owns the page
    GtkWidget *view = nullptr;   // not owned: the page owns the view
    GtkWidget *label = nullptr;  // not owned: the tab label box owns it
    GtkWidget *log_dot = nullptr;  // not owned: shown while logging
    std::unique_ptr<PtySession> pty;
    std::unique_ptr<SessionLog> log;  // null when logging is off

    explicit Tab(const Session &s) : session(s) {}
    // Members are destroyed in reverse order: the log closes before the pty.
    ~Tab() { g_print("~Tab %s\n", session.name.c_str()); }
};
