#include "app_window.h"

#include "session_item.h"

#include <initializer_list>

// Chapter 6: folders as a tree (GtkTreeListModel + GtkTreeExpander), a
// filter that rebuilds the root model, and expansion kept across rebuilds.

AppWindow::AppWindow(GtkApplication *app) {
    settings_ = load_settings(config_path("settings.ini"));
    sessions_path_ = config_path("sessions.ini");

    window_ = gtk_application_window_new(app);
    // The window owns this AppWindow: deleting it when the window is destroyed.
    g_object_set_data_full(G_OBJECT(window_), "app-window", this,
                           [](gpointer p) { delete static_cast<AppWindow *>(p); });

    build_ui();
    load_sessions_file();
    rebuild_tree({}, false);
}

AppWindow::~AppWindow() {
    g_clear_object(&tree_model_);
    g_clear_object(&root_);
}

void AppWindow::build_ui() {
    gtk_window_set_title(GTK_WINDOW(window_), "gtklab");
    gtk_window_set_default_size(GTK_WINDOW(window_), settings_.width, settings_.height);
    g_signal_connect(window_, "close-request", G_CALLBACK(on_close_request), this);

    stack_ = gtk_stack_new();
    gtk_stack_set_hhomogeneous(GTK_STACK(stack_), FALSE);  // measure only the visible page
    gtk_stack_set_vhomogeneous(GTK_STACK(stack_), FALSE);
    GtkWidget *hint = gtk_label_new("No tabs open");
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_stack_add_named(GTK_STACK(stack_), hint, "empty");
    notebook_ = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), gtk_label_new("A page"),
                             gtk_label_new("Tab 1"));
    gtk_stack_add_named(GTK_STACK(stack_), notebook_, "tabs");
    gtk_stack_set_visible_child_name(GTK_STACK(stack_), "empty");

    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *flip = gtk_button_new_with_label("Flip");
    g_signal_connect(flip, "clicked", G_CALLBACK(on_flip_clicked), this);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), flip);
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    paned_ = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned_), build_sidebar());
    gtk_paned_set_end_child(GTK_PANED(paned_), stack_);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned_), FALSE);
    gtk_paned_set_resize_start_child(GTK_PANED(paned_), FALSE);
    gtk_paned_set_position(GTK_PANED(paned_), settings_.sidebar_position);

    gtk_window_set_child(GTK_WINDOW(window_), paned_);
}

GtkWidget *AppWindow::build_sidebar() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);
    gtk_widget_set_margin_start(box, 6);
    gtk_widget_set_margin_end(box, 6);

    search_ = gtk_search_entry_new();
    g_signal_connect(search_, "search-changed", G_CALLBACK(on_search_changed), this);
    gtk_box_append(GTK_BOX(box), search_);

    // root (folders) -> tree list model (expands folders) -> selection -> view
    root_ = g_list_store_new(GTKLAB_TYPE_SESSION_ITEM);
    tree_model_ = gtk_tree_list_model_new(G_LIST_MODEL(g_object_ref(root_)),
                                          FALSE,  // passthrough: items are GtkTreeListRow
                                          FALSE,  // autoexpand
                                          create_child_model, this, nullptr);
    GtkSingleSelection *selection =
        gtk_single_selection_new(G_LIST_MODEL(g_object_ref(tree_model_)));

    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(on_factory_setup), this);
    g_signal_connect(factory, "bind", G_CALLBACK(on_factory_bind), this);
    g_signal_connect(factory, "unbind", G_CALLBACK(on_factory_unbind), this);

    list_view_ = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
    gtk_widget_add_css_class(list_view_, "navigation-sidebar");
    g_signal_connect(list_view_, "activate", G_CALLBACK(on_row_activated), this);

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), list_view_);
    gtk_box_append(GTK_BOX(box), scroller);
    return box;
}

void AppWindow::load_sessions_file() {
    if (!g_file_test(sessions_path_.c_str(), G_FILE_TEST_EXISTS)) {
        char *dir = g_path_get_dirname(sessions_path_.c_str());
        g_mkdir_with_parents(dir, 0700);
        g_free(dir);
        const std::string text = default_sessions_text();
        g_file_set_contents(sessions_path_.c_str(), text.c_str(),
                            static_cast<gssize>(text.size()), nullptr);
    }
    std::string err;
    folders_ = load_sessions(sessions_path_, err);
    if (!err.empty())
        g_printerr("sessions: %s\n", err.c_str());
}

// Called by GtkTreeListModel whenever it needs a row's children: for each
// folder row it shows (to decide whether to draw an expander) and again on
// expand. Return a new model each time (the tree model takes ownership), or
// nullptr for a leaf, which gets no expander.
GListModel *AppWindow::create_child_model(gpointer item, gpointer) {
    auto *si = GTKLAB_SESSION_ITEM(item);
    if (!gtklab_session_item_is_folder(si))
        return nullptr;
    const Folder &f = gtklab_session_item_folder(si);
    g_debug("create_child_model %s", f.name.c_str());

    GListStore *children = g_list_store_new(GTKLAB_TYPE_SESSION_ITEM);
    for (const Session &s : f.sessions) {
        GtklabSessionItem *child = gtklab_session_item_new_session(s);
        g_list_store_append(children, child);
        g_object_unref(child);
    }
    return G_LIST_MODEL(children);
}

// Names of the folders whose rows are expanded now.
std::set<std::string> AppWindow::capture_expanded() const {
    std::set<std::string> names;
    GListModel *rows = G_LIST_MODEL(tree_model_);
    const guint n = g_list_model_get_n_items(rows);
    for (guint i = 0; i < n; i++) {
        auto *row = GTK_TREE_LIST_ROW(g_list_model_get_item(rows, i));
        if (gtk_tree_list_row_get_depth(row) == 0 && gtk_tree_list_row_get_expanded(row)) {
            auto *si = GTKLAB_SESSION_ITEM(gtk_tree_list_row_get_item(row));
            names.insert(gtklab_session_item_folder(si).name);
            g_object_unref(si);
        }
        g_object_unref(row);
    }
    return names;
}

// Replace the root with the (filtered) folders, then re-open folders.
void AppWindow::rebuild_tree(const std::set<std::string> &expand, bool expand_all) {
    std::vector<Folder> shown = filter_sessions(folders_, filter_);

    // Build the new items first, then swap them in with one splice: one
    // items-changed signal instead of one per folder.
    std::vector<gpointer> items;
    for (const Folder &f : shown)
        items.push_back(gtklab_session_item_new_folder(f));
    g_list_store_splice(root_, 0, g_list_model_get_n_items(G_LIST_MODEL(root_)),
                        items.data(), static_cast<guint>(items.size()));
    for (gpointer p : items)
        g_object_unref(p);

    // Expanding a row inserts its children after it, so re-read the count
    // each time round instead of caching it.
    GListModel *rows = G_LIST_MODEL(tree_model_);
    for (guint i = 0; i < g_list_model_get_n_items(rows); i++) {
        auto *row = GTK_TREE_LIST_ROW(g_list_model_get_item(rows, i));
        if (gtk_tree_list_row_get_depth(row) == 0) {
            auto *si = GTKLAB_SESSION_ITEM(gtk_tree_list_row_get_item(row));
            if (expand_all || expand.count(gtklab_session_item_folder(si).name))
                gtk_tree_list_row_set_expanded(row, TRUE);
            g_object_unref(si);
        }
        g_object_unref(row);
    }
}

void AppWindow::on_search_changed(GtkSearchEntry *entry, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    const std::string text = gtk_editable_get_text(GTK_EDITABLE(entry));
    const bool was_filtering = !self->filter_.empty();

    if (!was_filtering && !text.empty())
        self->user_expanded_ = self->capture_expanded();  // remember the user's tree

    self->filter_ = text;
    if (text.empty())
        self->rebuild_tree(self->user_expanded_, false);  // back to exactly what was open
    else
        self->rebuild_tree({}, true);  // show every match
}

void AppWindow::on_row_activated(GtkListView *, guint position, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    auto *row = GTK_TREE_LIST_ROW(g_list_model_get_item(G_LIST_MODEL(self->tree_model_), position));
    auto *si = GTKLAB_SESSION_ITEM(gtk_tree_list_row_get_item(row));
    if (gtklab_session_item_is_folder(si))
        gtk_tree_list_row_set_expanded(row, !gtk_tree_list_row_get_expanded(row));
    else
        self->activate_session(gtklab_session_item_session(si));
    g_object_unref(si);
    g_object_unref(row);
}

void AppWindow::activate_session(const Session &s) {
    const char *shell = g_getenv("SHELL");
    std::string line;
    for (const std::string &arg : ssh_argv(s, shell ? shell : ""))
        line += (line.empty() ? "" : " ") + arg;
    g_print("open %s: %s\n", s.name.c_str(), line.c_str());  // Chapter 8 opens a tab
}

// --- factory ---------------------------------------------------------------

void AppWindow::on_factory_setup(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = gtk_image_new();
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *name = gtk_label_new(nullptr);
    GtkWidget *detail = gtk_label_new(nullptr);
    for (GtkWidget *l : {name, detail}) {
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    }
    gtk_widget_add_css_class(detail, "dim-label");
    gtk_widget_add_css_class(detail, "caption");
    gtk_box_append(GTK_BOX(text), name);
    gtk_box_append(GTK_BOX(text), detail);
    gtk_box_append(GTK_BOX(row), icon);
    gtk_box_append(GTK_BOX(row), text);

    // The expander draws the disclosure triangle and the indent for depth.
    GtkWidget *expander = gtk_tree_expander_new();
    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), row);

    g_object_set_data(G_OBJECT(expander), "icon", icon);
    g_object_set_data(G_OBJECT(expander), "name", name);
    g_object_set_data(G_OBJECT(expander), "detail", detail);
    gtk_list_item_set_child(item, expander);
}

void AppWindow::on_factory_bind(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *expander = gtk_list_item_get_child(item);
    auto *row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(item));
    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);

    auto *si = GTKLAB_SESSION_ITEM(gtk_tree_list_row_get_item(row));  // transfer full
    auto *icon = GTK_IMAGE(g_object_get_data(G_OBJECT(expander), "icon"));
    auto *name = GTK_LABEL(g_object_get_data(G_OBJECT(expander), "name"));
    auto *detail = GTK_LABEL(g_object_get_data(G_OBJECT(expander), "detail"));

    if (gtklab_session_item_is_folder(si)) {
        const Folder &f = gtklab_session_item_folder(si);
        g_debug("bind folder %s", f.name.c_str());
        gtk_image_set_from_icon_name(icon, "folder-symbolic");
        gtk_label_set_text(name, f.name.c_str());
        const std::string n = std::to_string(f.sessions.size()) +
                              (f.sessions.size() == 1 ? " session" : " sessions");
        gtk_label_set_text(detail, n.c_str());
    } else {
        const Session &s = gtklab_session_item_session(si);
        g_debug("bind %s", s.name.c_str());
        gtk_image_set_from_icon_name(icon, s.local ? "computer-symbolic" : "network-server-symbolic");
        gtk_label_set_text(name, s.name.c_str());
        std::string d;
        if (s.local) {
            d = "local";
        } else {
            d = s.user.empty() ? s.host : s.user + "@" + s.host;
            if (s.port != 22)
                d += ":" + std::to_string(s.port);
        }
        gtk_label_set_text(detail, d.c_str());
    }
    g_object_unref(si);
}

void AppWindow::on_factory_unbind(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *expander = gtk_list_item_get_child(item);
    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), nullptr);
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(expander), "name")), "");
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(expander), "detail")), "");
}

// --- window ----------------------------------------------------------------

gboolean AppWindow::on_close_request(GtkWindow *win, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    gtk_window_get_default_size(win, &self->settings_.width, &self->settings_.height);
    self->settings_.sidebar_position = gtk_paned_get_position(GTK_PANED(self->paned_));
    std::string err;
    if (!save_settings(config_path("settings.ini"), self->settings_, err))
        g_warning("settings not saved: %s", err.c_str());
    return FALSE;
}

void AppWindow::on_flip_clicked(GtkButton *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    GtkStack *stack = GTK_STACK(self->stack_);
    const char *now = gtk_stack_get_visible_child_name(stack);
    gtk_stack_set_visible_child_name(stack, g_strcmp0(now, "empty") == 0 ? "tabs" : "empty");
}
