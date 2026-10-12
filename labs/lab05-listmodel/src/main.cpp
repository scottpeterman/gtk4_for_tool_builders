// Lab 5: the list pipeline in isolation. Plain GObjects in a GListStore,
// a single selection, a list view and a factory; watch setup, bind and unbind.
#include <gtk/gtk.h>

struct Lab {
    GListStore *store = nullptr;
    GtkWidget *status = nullptr;
};

static void setup(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    gtk_list_item_set_child(item, gtk_label_new(""));
}

static void bind(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *label = gtk_list_item_get_child(item);
    auto *object = G_OBJECT(gtk_list_item_get_item(item));
    const char *name = static_cast<const char *>(g_object_get_data(object, "name"));
    gtk_label_set_text(GTK_LABEL(label), name ? name : "(unnamed)");
    g_print("bind: %s\n", name ? name : "(unnamed)");
}

static void unbind(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)), "");
    g_print("unbind\n");
}

static void add_item(GtkButton *, gpointer data) {
    auto *lab = static_cast<Lab *>(data);
    guint index = g_list_model_get_n_items(G_LIST_MODEL(lab->store)) + 1;
    GObject *object = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    char *name = g_strdup_printf("Session %u", index);
    g_object_set_data_full(object, "name", name, g_free);
    g_list_store_append(lab->store, object);
    g_object_unref(object); // Store now holds its own reference.
    gtk_label_set_text(GTK_LABEL(lab->status), "Item appended to model");
}

static void activate(GtkApplication *app, gpointer) {
    auto *lab = new Lab;
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "List Model Debugging Lab");
    gtk_window_set_default_size(GTK_WINDOW(window), 460, 320);
    g_object_set_data_full(G_OBJECT(window), "lab", lab, [](gpointer p) {
        auto *state = static_cast<Lab *>(p);
        g_clear_object(&state->store);
        delete state;
    });

    GtkWidget *column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(column, 12);
    gtk_widget_set_margin_bottom(column, 12);
    gtk_widget_set_margin_start(column, 12);
    gtk_widget_set_margin_end(column, 12);
    gtk_window_set_child(GTK_WINDOW(window), column);

    lab->store = g_list_store_new(G_TYPE_OBJECT);
    GtkSelectionModel *selection = GTK_SELECTION_MODEL(
        gtk_single_selection_new(G_LIST_MODEL(g_object_ref(lab->store))));
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup), nullptr);
    g_signal_connect(factory, "bind", G_CALLBACK(bind), nullptr);
    g_signal_connect(factory, "unbind", G_CALLBACK(unbind), nullptr);
    GtkWidget *view = gtk_list_view_new(selection, factory);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
    gtk_box_append(GTK_BOX(column), scroll);

    GtkWidget *button = gtk_button_new_with_label("Add session");
    g_signal_connect(button, "clicked", G_CALLBACK(add_item), lab);
    gtk_box_append(GTK_BOX(column), button);
    lab->status = gtk_label_new("Click Add session and inspect the callbacks");
    gtk_box_append(GTK_BOX(column), lab->status);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab.lab05", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}