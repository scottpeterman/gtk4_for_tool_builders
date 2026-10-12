// Lab 7: one command, three ways in. A button, a menu item and Ctrl+R all
// activate app.refresh; win.details is a stateful action.
#include <gtk/gtk.h>

static void on_refresh(GSimpleAction *, GVariant *, gpointer data) {
    GtkLabel *status = GTK_LABEL(data);
    gtk_label_set_text(status, "Refreshed through the action");
    g_print("app.refresh activated\n");
}

static void on_toggle(GSimpleAction *action, GVariant *, gpointer data) {
    GVariant *state = g_action_get_state(G_ACTION(action));  // a new reference
    gboolean enabled = !g_variant_get_boolean(state);
    g_variant_unref(state);
    g_simple_action_set_state(action, g_variant_new_boolean(enabled));
    gtk_label_set_text(GTK_LABEL(data), enabled ? "Details: ON" : "Details: OFF");
    g_print("win.details state: %s\n", enabled ? "true" : "false");
}

static void activate(GtkApplication *app, gpointer) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Actions Laboratory");
    gtk_window_set_default_size(GTK_WINDOW(window), 520, 280);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_window_set_child(GTK_WINDOW(window), box);

    GtkWidget *status = gtk_label_new("Choose a command");
    gtk_box_append(GTK_BOX(box), status);

    GSimpleAction *refresh = g_simple_action_new("refresh", nullptr);
    g_signal_connect(refresh, "activate", G_CALLBACK(on_refresh), status);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(refresh));
    g_object_unref(refresh);

    GSimpleAction *details = g_simple_action_new_stateful(
        "details", nullptr, g_variant_new_boolean(FALSE));
    g_signal_connect(details, "activate", G_CALLBACK(on_toggle), status);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(details));
    g_object_unref(details);

    GtkWidget *refresh_button = gtk_button_new_with_label("Refresh");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(refresh_button), "app.refresh");
    gtk_box_append(GTK_BOX(box), refresh_button);

    GtkWidget *details_button = gtk_button_new_with_label("Toggle details");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(details_button), "win.details");
    gtk_box_append(GTK_BOX(box), details_button);

    GMenu *menu = g_menu_new();
    g_menu_append(menu, "Refresh", "app.refresh");
    g_menu_append(menu, "Toggle details", "win.details");
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_menu_button_set_label(GTK_MENU_BUTTON(menu_button), "Commands");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
    gtk_box_append(GTK_BOX(box), menu_button);
    g_object_unref(menu);

    const char *shortcuts[] = {"<Control>r", nullptr};
    gtk_application_set_accels_for_action(app, "app.refresh", shortcuts);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(
        "io.github.gtklab.lab07", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}