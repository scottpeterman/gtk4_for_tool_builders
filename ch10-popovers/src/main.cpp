#include <gtk/gtk.h>

#include "app_window.h"

static void on_activate(GtkApplication *app, gpointer) {
    // The window owns the AppWindow (see its constructor).
    auto *w = new AppWindow(app);
    gtk_window_present(w->window());
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
