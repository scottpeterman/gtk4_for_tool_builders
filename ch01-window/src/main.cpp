#include <gtk/gtk.h>

static gboolean on_close_request(GtkWindow *win, gpointer) {
    int w = 0, h = 0;
    gtk_window_get_default_size(win, &w, &h);
    g_print("closing at %dx%d\n", w, h);
    return FALSE;  // FALSE lets the window close
}

static void on_activate(GtkApplication *app, gpointer) {
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "gtklab");
    gtk_window_set_default_size(GTK_WINDOW(win), 900, 600);
    g_signal_connect(win, "close-request", G_CALLBACK(on_close_request), nullptr);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
