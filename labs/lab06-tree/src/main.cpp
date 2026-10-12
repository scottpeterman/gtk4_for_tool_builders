// Lab 6: a tree is a list that grows. A GtkTreeListModel over string items,
// with a child model per group; watch when children_for and bind run.
#include <gtk/gtk.h>

// Called by the tree list model whenever it needs to know an item's
// children: for every visible row, so the expander knows whether to draw a
// triangle, and again on expand. Return a new reference, or nullptr for a leaf.
static GListModel *children_for(gpointer item, gpointer) {
    const char *name = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
    GListModel *children = G_LIST_MODEL(g_object_get_data(G_OBJECT(item), "children"));
    g_print("children_for: %s -> %s\n", name, children ? "model" : "leaf");
    if (children == nullptr)
        return nullptr;
    return G_LIST_MODEL(g_object_ref(children));
}

static void setup_row(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *expander = gtk_tree_expander_new();
    GtkWidget *label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), label);
    gtk_list_item_set_child(item, expander);
    g_print("setup: row widget created\n");
}

static void bind_row(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkTreeListRow *row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(item));
    GtkStringObject *data = GTK_STRING_OBJECT(gtk_tree_list_row_get_item(row));
    GtkWidget *expander = gtk_list_item_get_child(item);
    GtkWidget *label = gtk_tree_expander_get_child(GTK_TREE_EXPANDER(expander));
    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);
    gtk_label_set_text(GTK_LABEL(label), gtk_string_object_get_string(data));
    g_print("bind: %s depth=%u\n", gtk_string_object_get_string(data),
            gtk_tree_list_row_get_depth(row));
}

static void unbind_row(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *expander = gtk_list_item_get_child(item);
    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), nullptr);
    g_print("unbind: row widget released from item\n");
}

static void add_leaf(GListStore *store, const char *name) {
    GtkStringObject *item = gtk_string_object_new(name);
    g_list_store_append(store, item);
    g_object_unref(item);
}

static void add_group(GListStore *roots, const char *name,
                      const char *first, const char *second) {
    GtkStringObject *group = gtk_string_object_new(name);
    GListStore *children = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    add_leaf(children, first);
    add_leaf(children, second);
    g_object_set_data_full(G_OBJECT(group), "children", children,
                           (GDestroyNotify)g_object_unref);
    g_list_store_append(roots, group);
    g_object_unref(group);
}

static void activate(GtkApplication *app, gpointer) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Tree Model Debugging Lab");
    gtk_window_set_default_size(GTK_WINDOW(window), 540, 360);

    GListStore *roots = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    add_group(roots, "Production", "Router A", "Switch B");
    add_group(roots, "Laboratory", "Firewall C", "Host D");

    GtkTreeListModel *tree = gtk_tree_list_model_new(
        G_LIST_MODEL(roots), FALSE, FALSE, children_for, nullptr, nullptr);  // passthrough, autoexpand
    GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(tree));
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_row), nullptr);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_row), nullptr);
    g_signal_connect(factory, "unbind", G_CALLBACK(unbind_row), nullptr);

    GtkWidget *view = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
    gtk_window_set_child(GTK_WINDOW(window), scroll);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab.lab06",
                                               G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}