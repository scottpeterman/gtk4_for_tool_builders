#include <gtk/gtk.h>
#include <cstdio>

struct Counter {
    int clicks = 0;
    ~Counter() { g_print("Counter freed after %d clicks\n", clicks); }
};

static void on_count_clicked(GtkButton *button, gpointer) {
    auto *c = static_cast<Counter *>(g_object_get_data(G_OBJECT(button), "counter"));
    c->clicks++;
    char label[32];
    std::snprintf(label, sizeof label, "Clicked %d", c->clicks);
    gtk_button_set_label(button, label);
}

// user_data is the counter button; g_signal_connect_object disconnects
// this handler when that button is finalised.
static void on_remove_clicked(GtkButton *, gpointer user_data) {
    GtkWidget *count = GTK_WIDGET(user_data);
    GtkWidget *box = gtk_widget_get_parent(count);
    if (box)
        gtk_box_remove(GTK_BOX(box), count);
}

static gboolean on_close_request(GtkWindow *win, gpointer) {
    int w = 0, h = 0;
    gtk_window_get_default_size(win, &w, &h);
    g_print("closing at %dx%d\n", w, h);
    return FALSE;
}

static void on_activate(GtkApplication *app, gpointer) {
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "gtklab");
    gtk_window_set_default_size(GTK_WINDOW(win), 900, 600);
    g_signal_connect(win, "close-request", G_CALLBACK(on_close_request), nullptr);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);

    GtkWidget *count = gtk_button_new_with_label("Clicked 0");
    g_object_set_data_full(G_OBJECT(count), "counter", new Counter,
                           [](gpointer p) { delete static_cast<Counter *>(p); });
    g_signal_connect(count, "clicked", G_CALLBACK(on_count_clicked), nullptr);

    GtkWidget *remove = gtk_button_new_with_label("Remove counter");
    g_signal_connect_object(remove, "clicked", G_CALLBACK(on_remove_clicked), count,
                            GConnectFlags(0));

    gtk_box_append(GTK_BOX(box), count);
    gtk_box_append(GTK_BOX(box), remove);
    gtk_window_set_child(GTK_WINDOW(win), box);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
