// Tab: everything one notebook page owns. The page owns its Tab through
// g_object_set_data_full, so removing the page deletes the Tab.
#pragma once

#include <gtk/gtk.h>

#include "core/session.h"

struct Tab {
    Session session;
    GtkWidget *page = nullptr;   // not owned: the notebook owns the page
    GtkWidget *view = nullptr;   // not owned: the page owns the view
    GtkWidget *label = nullptr;  // not owned: the tab label box owns it

    explicit Tab(const Session &s) : session(s) {}
    ~Tab() { g_print("~Tab %s\n", session.name.c_str()); }  // Chapter 11: closes the pty here
};
