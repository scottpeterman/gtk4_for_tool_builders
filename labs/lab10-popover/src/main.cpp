// Lab 10: a popover you parent yourself, and a dialog that answers later.
// The main loop keeps running while the dialog is open.
#include <gtk/gtk.h>

struct Lab {
    GtkWidget *window = nullptr;
    GtkWidget *button = nullptr;
    GtkWidget *popover = nullptr;
    GtkWidget *status = nullptr;
    unsigned confirmations = 0;
};

static void on_dialog_response(GtkDialog *dialog, int response, gpointer data) {
    auto *lab = static_cast<Lab *>(data);
    if (response == GTK_RESPONSE_ACCEPT) {
        lab->confirmations++;
        char message[80];
        g_snprintf(message, sizeof(message), "Confirmed %u time(s)", lab->confirmations);
        gtk_label_set_text(GTK_LABEL(lab->status), message);
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_confirm_clicked(GtkButton *, gpointer data) {
    auto *lab = static_cast<Lab *>(data);
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(lab->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
        "%s", "Apply the change?");
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog), "%s", "The response will arrive asynchronously.");
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Apply", GTK_RESPONSE_ACCEPT);
    g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), lab);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_popover_closed(GtkPopover *, gpointer) {
    g_print("Popover closed; the main loop continues.\n");
}

static void on_menu_clicked(GtkButton *, gpointer data) {
    auto *lab = static_cast<Lab *>(data);
    gtk_popover_popup(GTK_POPOVER(lab->popover));
}

static void on_activate(GtkApplication *app, gpointer) {
    auto *lab = new Lab();
    lab->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(lab->window), "Chapter 10 laboratory");
    gtk_window_set_default_size(GTK_WINDOW(lab->window), 420, 240);
    g_object_set_data_full(G_OBJECT(lab->window), "lab", lab,
        [](gpointer p) { delete static_cast<Lab *>(p); });

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_window_set_child(GTK_WINDOW(lab->window), box);

    lab->button = gtk_button_new_with_label("Show popover");
    gtk_box_append(GTK_BOX(box), lab->button);
    lab->popover = gtk_popover_new();
    GtkWidget *popover_label = gtk_label_new("This is contextual UI, not a new window.");
    gtk_widget_set_margin_top(popover_label, 12);
    gtk_widget_set_margin_bottom(popover_label, 12);
    gtk_widget_set_margin_start(popover_label, 12);
    gtk_widget_set_margin_end(popover_label, 12);
    gtk_popover_set_child(GTK_POPOVER(lab->popover), popover_label);
    // A popover is not a child the button knows how to drop: whoever calls
    // gtk_widget_set_parent must call gtk_widget_unparent. Do it when the
    // button is destroyed, or GTK warns "Finalizing GtkButton ... but it
    // still has children left" at quit.
    gtk_widget_set_parent(lab->popover, lab->button);
    g_signal_connect(lab->button, "destroy", G_CALLBACK(+[](GtkWidget *, gpointer popover) {
                         gtk_widget_unparent(GTK_WIDGET(popover));
                     }),
                     lab->popover);
    g_signal_connect(lab->popover, "closed", G_CALLBACK(on_popover_closed), lab);
    g_signal_connect(lab->button, "clicked", G_CALLBACK(on_menu_clicked), lab);

    GtkWidget *confirm = gtk_button_new_with_label("Open confirmation dialog");
    gtk_box_append(GTK_BOX(box), confirm);
    g_signal_connect(confirm, "clicked", G_CALLBACK(on_confirm_clicked), lab);
    lab->status = gtk_label_new("No confirmations yet");
    gtk_box_append(GTK_BOX(box), lab->status);
    gtk_window_present(GTK_WINDOW(lab->window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab.lab10", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}