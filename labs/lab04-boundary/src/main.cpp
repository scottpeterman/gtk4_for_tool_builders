// Lab 4: the callback is an adapter. It turns a GTK signal into a model call,
// then refreshes the view from the model. The model knows nothing of GTK.
#include <gtk/gtk.h>
#include <string>

#include "core/counter_model.h"

struct WindowState {
    CounterModel model;
    GtkWidget *label = nullptr;  // borrowed: the window's widget tree owns it
};

static void refresh_view(WindowState *state) {
    std::string text = state->model.description();
    gtk_label_set_text(GTK_LABEL(state->label), text.c_str());  // the label copies the text
}

static void on_increment(GtkButton *, gpointer data) {
    auto *state = static_cast<WindowState *>(data);
    state->model.increment();
    refresh_view(state);
}

static void on_reset(GtkButton *, gpointer data) {
    auto *state = static_cast<WindowState *>(data);
    state->model.reset();
    refresh_view(state);
}

static void on_activate(GtkApplication *app, gpointer) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Separation Lab");
    gtk_window_set_default_size(GTK_WINDOW(window), 420, 180);

    // The window owns the state: it is deleted when the window is finalized,
    // after its buttons (and their handlers) are gone.
    auto *state = new WindowState;
    g_object_set_data_full(G_OBJECT(window), "state", state,
                           [](gpointer p) { delete static_cast<WindowState *>(p); });

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_window_set_child(GTK_WINDOW(window), box);

    state->label = gtk_label_new("");
    gtk_box_append(GTK_BOX(box), state->label);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), buttons);
    GtkWidget *increment = gtk_button_new_with_label("Increment");
    GtkWidget *reset = gtk_button_new_with_label("Reset");
    gtk_box_append(GTK_BOX(buttons), increment);
    gtk_box_append(GTK_BOX(buttons), reset);

    g_signal_connect(increment, "clicked", G_CALLBACK(on_increment), state);
    g_signal_connect(reset, "clicked", G_CALLBACK(on_reset), state);
    refresh_view(state);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab.lab04", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
