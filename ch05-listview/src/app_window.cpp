#include "app_window.h"

#include "session_item.h"

#include <initializer_list>

// Chapter 5: a flat list of every session in a GtkListView.

AppWindow::AppWindow(GtkApplication *app) {
    settings_ = load_settings(config_path("settings.ini"));
    sessions_path_ = config_path("sessions.ini");

    window_ = gtk_application_window_new(app);
    // The window owns this AppWindow: deleting it when the window is destroyed.
    g_object_set_data_full(G_OBJECT(window_), "app-window", this,
                           [](gpointer p) { delete static_cast<AppWindow *>(p); });

    build_ui();
    load_sessions_file();
    fill_list();
}

AppWindow::~AppWindow() {
    g_clear_object(&store_);
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

    gtk_box_append(GTK_BOX(box), gtk_search_entry_new());

    // Model -> selection -> view, with a factory making row widgets.
    store_ = g_list_store_new(GTKLAB_TYPE_SESSION_ITEM);
    // The selection model takes ownership of the model it wraps, so give it
    // a reference of its own; store_ keeps ours for fill_list().
    GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(store_)));

    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(on_factory_setup), this);
    g_signal_connect(factory, "bind", G_CALLBACK(on_factory_bind), this);
    g_signal_connect(factory, "unbind", G_CALLBACK(on_factory_unbind), this);

    // gtk_list_view_new takes ownership of both arguments.
    list_view_ = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
    gtk_widget_add_css_class(list_view_, "navigation-sidebar");

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

void AppWindow::fill_list() {
    g_list_store_remove_all(store_);
    for (const Folder &f : folders_) {
        for (const Session &s : f.sessions) {
            GtklabSessionItem *item = gtklab_session_item_new_session(s);
            g_list_store_append(store_, item);
            g_object_unref(item);  // the store holds its own reference
        }
    }
}

// --- factory ---------------------------------------------------------------

// setup: build the row widgets once. No item yet; nothing to fill.
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

    // Non-owning pointers for bind; the row owns its children.
    g_object_set_data(G_OBJECT(row), "icon", icon);
    g_object_set_data(G_OBJECT(row), "name", name);
    g_object_set_data(G_OBJECT(row), "detail", detail);
    gtk_list_item_set_child(item, row);
}

// bind: fill the existing row widgets for one item. Runs as rows scroll in.
void AppWindow::on_factory_bind(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *row = gtk_list_item_get_child(item);
    auto *si = GTKLAB_SESSION_ITEM(gtk_list_item_get_item(item));
    const Session &s = gtklab_session_item_session(si);
    g_debug("bind %s", s.name.c_str());

    gtk_image_set_from_icon_name(GTK_IMAGE(g_object_get_data(G_OBJECT(row), "icon")),
                                 s.local ? "computer-symbolic" : "network-server-symbolic");
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(row), "name")), s.name.c_str());

    std::string detail;
    if (s.local) {
        detail = "local";
    } else {
        detail = s.user.empty() ? s.host : s.user + "@" + s.host;
        if (s.port != 22)
            detail += ":" + std::to_string(s.port);
    }
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(row), "detail")), detail.c_str());
}

// unbind: the row is about to show a different item (or none). Clear
// anything that refers to the old one.
void AppWindow::on_factory_unbind(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
    GtkWidget *row = gtk_list_item_get_child(item);
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(row), "name")), "");
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(row), "detail")), "");
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
