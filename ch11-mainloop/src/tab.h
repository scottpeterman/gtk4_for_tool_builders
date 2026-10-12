// Tab: everything one notebook page owns. The page owns its Tab through
// g_object_set_data_full, so removing the page deletes the Tab, and with it
// the pty session.
#pragma once

#include <gtk/gtk.h>

#include <memory>

#include "core/session.h"
#include "pty_session.h"

struct Tab {
    Session session;
    GtkWidget *page = nullptr;   // not owned: the notebook owns the page
    GtkWidget *view = nullptr;   // not owned: the page owns the view
    GtkWidget *entry = nullptr;  // not owned
    GtkWidget *label = nullptr;  // not owned: the tab label box owns it
    int zoom = 100;              // percent; Chapter 14 turns this into a font size
    std::unique_ptr<PtySession> pty;

    explicit Tab(const Session &s) : session(s) {}
    ~Tab() { g_print("~Tab %s\n", session.name.c_str()); }  // pty is closed by its destructor
};
