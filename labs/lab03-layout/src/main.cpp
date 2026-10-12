// Lab 3: layout policy in isolation. A box, a paned and a stack; drag the
// divider, resize the window and flip pages to see who decides the sizes.
#include <gtk/gtk.h>

static void on_toggled(GtkToggleButton *button, gpointer user_data) {
    GtkStack *stack = GTK_STACK(user_data);
    gboolean active = gtk_toggle_button_get_active(button);
    const char *page = active ? "details" : "welcome";
    g_print("Switching to page: %s\n", page);
    gtk_stack_set_visible_child_name(stack, page);
}

static void on_activate(GtkApplication *app, gpointer) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Layout Laboratory");
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 360);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(root, 16);
    gtk_widget_set_margin_bottom(root, 16);
    gtk_widget_set_margin_start(root, 16);
    gtk_widget_set_margin_end(root, 16);
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget *toggle = gtk_toggle_button_new_with_label("Show details");
    gtk_box_append(GTK_BOX(root), toggle);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(root), paned);

    GtkWidget *sidebar = gtk_label_new("Sidebar: fixed purpose");
    gtk_widget_set_size_request(sidebar, 170, -1);
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(stack), gtk_label_new("Welcome page"), "welcome");
    gtk_stack_add_named(GTK_STACK(stack), gtk_label_new("Details page"), "details");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "welcome");
    gtk_paned_set_end_child(GTK_PANED(paned), stack);
    gtk_paned_set_position(GTK_PANED(paned), 190);

    g_signal_connect(toggle, "toggled", G_CALLBACK(on_toggled), stack);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab.lab03",
                                               G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}