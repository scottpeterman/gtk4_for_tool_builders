#include "app_window.h"

#include "gtkpyte-terminal.h"
#include "session_item.h"
#include "theme_apply.h"

#include <initializer_list>

// Chapter 15: resources. Themes, CSS and an icon compiled into the binary.

AppWindow::AppWindow(GtkApplication *app) {
    settings_ = load_settings(config_path("settings.ini"));
    sessions_path_ = config_path("sessions.ini");

    themes_ = load_themes(themes_error_);
    install_style();

    window_ = gtk_application_window_new(app);
    // The window owns this AppWindow: deleting it when the window is destroyed.
    g_object_set_data_full(G_OBJECT(window_), "app-window", this,
                           [](gpointer p) { delete static_cast<AppWindow *>(p); });

    build_ui();
    install_actions(app);
    set_app_variant(settings_.app_variant);
    load_sessions_file();
    if (!themes_error_.empty()) {  // say so rather than start with no themes and no reason
        gtk_label_set_text(GTK_LABEL(error_label_), themes_error_.c_str());
        gtk_widget_set_visible(error_label_, TRUE);
    }
    rebuild_tree({}, false);
    watch_sessions_file();
}

AppWindow::~AppWindow() {
    // A pending timeout holds `this`. Remove it, or it fires on freed memory.
    if (save_source_)
        g_source_remove(save_source_);
    if (reload_source_)
        g_source_remove(reload_source_);
    if (sessions_monitor_) {
        g_signal_handlers_disconnect_by_data(sessions_monitor_, this);
        g_file_monitor_cancel(sessions_monitor_);
        g_clear_object(&sessions_monitor_);
    }
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
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook_), TRUE);
    // These handlers hold `this`. Chapter 16 comes back to that.
    g_signal_connect(notebook_, "page-added", G_CALLBACK(on_page_added), this);
    g_signal_connect(notebook_, "page-removed", G_CALLBACK(on_page_removed), this);
    g_signal_connect(notebook_, "switch-page", G_CALLBACK(on_switch_page), this);
    gtk_stack_add_named(GTK_STACK(stack_), notebook_, "tabs");
    gtk_stack_set_visible_child_name(GTK_STACK(stack_), "empty");

    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
    GMenuModel *menu = build_menu();
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), menu);  // takes its own ref
    g_object_unref(menu);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menu_button);
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    sidebar_ = build_sidebar();
    gtk_widget_set_visible(sidebar_, settings_.sidebar_visible);
    paned_ = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned_), sidebar_);
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

    // Shown only when the sessions file cannot be read or parsed.
    error_label_ = gtk_label_new(nullptr);
    gtk_label_set_wrap(GTK_LABEL(error_label_), TRUE);
    gtk_label_set_xalign(GTK_LABEL(error_label_), 0);
    gtk_widget_add_css_class(error_label_, "error");
    gtk_widget_add_css_class(error_label_, "banner");
    gtk_widget_set_visible(error_label_, FALSE);
    gtk_box_append(GTK_BOX(box), error_label_);

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
    auto loaded = load_sessions(sessions_path_, err);
    if (err.empty()) {
        folders_ = std::move(loaded);
        gtk_widget_set_visible(error_label_, FALSE);
    } else {
        // Keep the last good tree; say what is wrong above it.
        const std::string msg = "Sessions file: " + err;
        gtk_label_set_text(GTK_LABEL(error_label_), msg.c_str());
        gtk_widget_set_visible(error_label_, TRUE);
    }
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
    add_tab(s);
}

// --- tabs --------------------------------------------------------------------

// Focus on idle. A tab opened by double-clicking the tree is created while
// that click is still being handled; the list view takes focus back when
// the click finishes, so grabbing focus now would be undone.
static void focus_later(GtkWidget *widget) {
    g_idle_add(
        [](gpointer p) -> gboolean {
            GtkWidget *w = GTK_WIDGET(p);
            if (gtk_widget_get_root(w))  // still in a window
                gtk_widget_grab_focus(w);
            g_object_unref(w);
            return G_SOURCE_REMOVE;
        },
        g_object_ref(widget));
}

Tab *AppWindow::add_tab(const Session &s) {
    auto *tab = new Tab(s);

    // Output view: our character-cell widget, scrollable through its
    // GtkScrollable adjustments. The scrolled window is the notebook page.
    tab->view = gtkpyte_terminal_new();
    apply_font(tab);
    apply_terminal_theme(tab);
    tab->page = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->page), GTK_POLICY_NEVER,
                                   GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab->page), tab->view);

    // Capture phase on the page: these run before anything inside it.
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);  // all buttons
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(click), GTK_PHASE_CAPTURE);
    g_signal_connect(click, "pressed", G_CALLBACK(on_page_pressed), tab);
    gtk_widget_add_controller(tab->page, GTK_EVENT_CONTROLLER(click));

    GtkEventController *scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_event_controller_set_propagation_phase(scroll, GTK_PHASE_CAPTURE);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_zoom_scroll), tab);
    gtk_widget_add_controller(tab->page, scroll);

    g_object_set_data(G_OBJECT(tab->page), "app-window", this);  // not owned

    // The page owns the Tab: removing the page deletes it.
    g_object_set_data_full(G_OBJECT(tab->page), "tab", tab,
                           [](gpointer p) { delete static_cast<Tab *>(p); });

    // Tab label: name + close button.
    GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    tab->log_dot = gtk_image_new_from_icon_name("gtklab-log-symbolic");  // from our resources
    gtk_widget_add_css_class(tab->log_dot, "log-indicator");
    gtk_widget_set_tooltip_text(tab->log_dot, "Logging");
    gtk_widget_set_visible(tab->log_dot, FALSE);
    gtk_box_append(GTK_BOX(label_box), tab->log_dot);
    tab->label = gtk_label_new(s.name.c_str());
    GtkWidget *close = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
    gtk_widget_set_tooltip_text(close, "Close tab");
    g_signal_connect_object(close, "clicked", G_CALLBACK(on_tab_close_clicked), tab->page,
                            GConnectFlags(0));
    gtk_box_append(GTK_BOX(label_box), tab->label);
    gtk_box_append(GTK_BOX(label_box), close);

    const int n = gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), tab->page, label_box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook_), tab->page, TRUE);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), n);
    start_tab(tab);
    focus_later(tab->view);
    return tab;
}

void AppWindow::start_tab(Tab *tab) {
    tab->pty = std::make_unique<PtySession>();
    GtkpyteTerminal *term = GTKPYTE_TERMINAL(tab->view);
    // The log tap sees raw bytes before the terminal does.
    tab->pty->set_output_handler([tab, term](const char *data, gsize len) {
        if (tab->log)
            tab->log->write(data, len);
        gtkpyte_terminal_feed(term, data, len);
    });

    // Keys and pastes from the grid go to the child.
    g_signal_connect(tab->view, "commit",
                     G_CALLBACK(+[](GtkpyteTerminal *, GBytes *bytes, gpointer data) {
                         gsize len = 0;
                         const char *p = static_cast<const char *>(g_bytes_get_data(bytes, &len));
                         static_cast<Tab *>(data)->pty->write(p, len);
                     }),
                     tab);

    // The grid changed shape: tell the child (TIOCSWINSZ -> SIGWINCH).
    g_signal_connect(tab->view, "resized",
                     G_CALLBACK(+[](GtkpyteTerminal *, int cols, int rows, gpointer data) {
                         static_cast<Tab *>(data)->pty->resize(cols, rows);
                     }),
                     tab);

    // When the child exits, close the tab, but not from inside this
    // callback: removing the page deletes the Tab, which deletes the
    // PtySession whose callback is running. Defer to an idle, holding a
    // reference to the page so it cannot vanish in between.
    GtkWidget *page = tab->page;
    tab->pty->set_exit_handler([page](int status) {
        g_debug("child exited, status %d; closing tab on idle", status);
        g_idle_add(
            [](gpointer p) -> gboolean {
                GtkWidget *pg = GTK_WIDGET(p);
                if (GtkWidget *nb = gtk_widget_get_ancestor(pg, GTK_TYPE_NOTEBOOK)) {
                    const int n = gtk_notebook_page_num(GTK_NOTEBOOK(nb), pg);
                    if (n >= 0)
                        gtk_notebook_remove_page(GTK_NOTEBOOK(nb), n);
                }
                g_object_unref(pg);
                return G_SOURCE_REMOVE;
            },
            g_object_ref(page));
    });

    const char *shell = g_getenv("SHELL");
    // A real emulator now: tell the child so.
    if (!tab->pty->start(ssh_argv(tab->session, shell ? shell : ""), gtkpyte_terminal_get_columns(term),
                         gtkpyte_terminal_get_rows(term), "xterm-256color")) {
        const std::string msg = "Could not start: " + tab->pty->error() + "\r\n";
        gtkpyte_terminal_feed(term, msg.c_str(), msg.size());
    }
}

// --- sessions file monitor -------------------------------------------------

void AppWindow::watch_sessions_file() {
    GFile *file = g_file_new_for_path(sessions_path_.c_str());
    sessions_monitor_ = g_file_monitor_file(file, G_FILE_MONITOR_NONE, nullptr, nullptr);
    g_object_unref(file);
    if (sessions_monitor_)
        g_signal_connect(sessions_monitor_, "changed", G_CALLBACK(on_sessions_changed), this);
}

// Editors save in several steps (write temp, rename, chmod). Each restarts
// a 300 ms timer, so the burst becomes one reload.
void AppWindow::on_sessions_changed(GFileMonitor *, GFile *, GFile *, GFileMonitorEvent event,
                                    gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    g_debug("sessions file event %d", event);
    if (self->reload_source_)
        g_source_remove(self->reload_source_);
    self->reload_source_ = g_timeout_add(300, on_sessions_settled, self);
}

gboolean AppWindow::on_sessions_settled(gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    self->reload_source_ = 0;
    g_debug("sessions file settled; reloading");
    self->load_sessions_file();
    self->rebuild_tree(self->capture_expanded(), !self->filter_.empty());
    return G_SOURCE_REMOVE;
}

void AppWindow::on_tab_close_clicked(GtkButton *, gpointer page_ptr) {
    GtkWidget *page = GTK_WIDGET(page_ptr);
    // The page's parent is the notebook's internal stack, not the notebook.
    GtkWidget *notebook = gtk_widget_get_ancestor(page, GTK_TYPE_NOTEBOOK);
    if (!notebook)
        return;
    const int n = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), page);
    if (n >= 0)
        gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), n);
}

// --- input -------------------------------------------------------------------

void AppWindow::on_page_pressed(GtkGestureClick *gesture, int, double x, double y, gpointer data) {
    auto *tab = static_cast<Tab *>(data);
    const guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    if (button == GDK_BUTTON_SECONDARY) {
        // Claim: no other gesture sees this click. Chapter 10 shows a menu here.
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        g_debug("page capture: button %u claimed (menu at %.0f,%.0f) [%s]", button, x, y,
                tab->session.name.c_str());
        auto *self = static_cast<AppWindow *>(g_object_get_data(G_OBJECT(tab->page), "app-window"));
        self->show_terminal_menu(tab, x, y);
    } else {
        // Deny: step aside so the widgets inside get the click.
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_DENIED);
        g_debug("page capture: button %u denied", button);
    }
}

gboolean AppWindow::on_zoom_scroll(GtkEventControllerScroll *ctl, double, double dy, gpointer data) {
    auto *tab = static_cast<Tab *>(data);
    GdkModifierType mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(ctl));
    if (!(mods & GDK_CONTROL_MASK)) {
        g_debug("zoom scroll: plain wheel, passing it on");
        return FALSE;  // not ours: the terminal scrolls its scrollback
    }
    auto *self = static_cast<AppWindow *>(g_object_get_data(G_OBJECT(tab->page), "app-window"));
    self->set_font_size(self->settings_.font_size + (dy < 0 ? 1 : -1));
    return TRUE;
}

void AppWindow::on_page_added(GtkNotebook *, GtkWidget *, guint, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    gtk_stack_set_visible_child_name(GTK_STACK(self->stack_), "tabs");
}

void AppWindow::on_page_removed(GtkNotebook *nb, GtkWidget *, guint, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    g_debug("page-removed, %d left", gtk_notebook_get_n_pages(nb));
    if (gtk_notebook_get_n_pages(nb) == 0)
        gtk_stack_set_visible_child_name(GTK_STACK(self->stack_), "empty");
}

// --- factory ---------------------------------------------------------------

void AppWindow::on_factory_setup(GtkSignalListItemFactory *, GtkListItem *item, gpointer self) {
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

    // Right-click on a row: the handler needs the list item to find the
    // session under the pointer at click time (rows are recycled).
    GtkGesture *rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rclick), GDK_BUTTON_SECONDARY);
    g_object_set_data(G_OBJECT(rclick), "list-item", item);
    g_signal_connect(rclick, "pressed", G_CALLBACK(on_row_pressed), self);
    gtk_widget_add_controller(expander, GTK_EVENT_CONTROLLER(rclick));

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
    if (gtk_widget_get_visible(self->sidebar_))
        self->settings_.sidebar_position = gtk_paned_get_position(GTK_PANED(self->paned_));
    self->save_settings_now();
    return FALSE;
}

// --- actions, menu, shortcuts ------------------------------------------------

// One table drives the accelerators shown in menus and the capture-phase
// shortcut controller.
struct Accel {
    const char *action;
    const char *trigger;
};
static const Accel kAccels[] = {
    {"win.quit", "<Control><Shift>q"},
    {"win.reload", "<Control><Shift>r"},
    {"win.toggle-sidebar", "<Control><Shift>b"},
    {"win.open-sessions-file", "<Control><Shift>e"},
    {"win.copy", "<Control><Shift>c"},
    {"win.paste", "<Control><Shift>v"},
    {"win.select-all", "<Control><Shift>a"},
    {"win.log", "<Control><Shift>l"},
    {"win.zoom-in", "<Control>equal"},
    {"win.zoom-out", "<Control>minus"},
    {"win.zoom-reset", "<Control>0"},
    {"win.close-tab", "<Control><Shift>w"},
    {"win.next-tab", "<Control>Page_Down"},
    {"win.prev-tab", "<Control>Page_Up"},
};

void AppWindow::install_actions(GtkApplication *app) {
    static const GActionEntry entries[] = {
        // name, activate, parameter type, initial state, change_state
        {"quit", act_quit, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"reload", act_reload, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"open-sessions-file", act_open_sessions_file, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"open-session", act_open_session, "s", nullptr, nullptr, {0, 0, 0}},
        {"copy", act_copy, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"paste", act_paste, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"select-all", act_select_all, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"log", nullptr, nullptr, "false", act_log, {0, 0, 0}},
        {"zoom-in", act_zoom, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"terminal-theme", nullptr, "s", "'dark'", act_terminal_theme, {0, 0, 0}},
        {"zoom-out", act_zoom, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"zoom-reset", act_zoom, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"close-tab", act_close_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"close-other-tabs", act_close_other_tabs, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"next-tab", act_next_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"prev-tab", act_prev_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"import-sessions", act_import_file, nullptr, nullptr, nullptr, {0, 0, 0}},
        // A boolean state and no parameter: activating toggles it, then
        // change_state runs with the new value.
        {"toggle-sidebar", nullptr, nullptr, "true", act_toggle_sidebar, {0, 0, 0}},
        // A string parameter and state: a radio group in menus.
        {"app-variant", nullptr, "s", "'light'", act_app_variant, {0, 0, 0}},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(window_), entries, G_N_ELEMENTS(entries), this);

    // Initial states from settings.
    GAction *a = g_action_map_lookup_action(G_ACTION_MAP(window_), "toggle-sidebar");
    g_simple_action_set_state(G_SIMPLE_ACTION(a), g_variant_new_boolean(settings_.sidebar_visible));
    a = g_action_map_lookup_action(G_ACTION_MAP(window_), "app-variant");
    g_simple_action_set_state(G_SIMPLE_ACTION(a), g_variant_new_string(settings_.app_variant.c_str()));
    a = g_action_map_lookup_action(G_ACTION_MAP(window_), "terminal-theme");
    g_simple_action_set_state(G_SIMPLE_ACTION(a), g_variant_new_string(settings_.terminal_theme.c_str()));

    // 1) Application accelerators: menus display them next to the item.
    // 2) The same triggers in a capture-phase controller on the window, so
    //    they run before the focused widget (later, the terminal) sees the key.
    GtkEventController *shortcuts = gtk_shortcut_controller_new();
    gtk_event_controller_set_propagation_phase(shortcuts, GTK_PHASE_CAPTURE);
    for (const Accel &acc : kAccels) {
        const char *accels[] = {acc.trigger, nullptr};
        gtk_application_set_accels_for_action(app, acc.action, accels);
        gtk_shortcut_controller_add_shortcut(
            GTK_SHORTCUT_CONTROLLER(shortcuts),
            gtk_shortcut_new(gtk_shortcut_trigger_parse_string(acc.trigger),
                             gtk_named_action_new(acc.action)));
    }
    gtk_widget_add_controller(window_, shortcuts);
}

GMenuModel *AppWindow::build_menu() {
    GMenu *menu = g_menu_new();

    GMenu *top = g_menu_new();
    g_menu_append(top, "Reload Sessions", "win.reload");
    g_menu_append(top, "Edit Sessions File", "win.open-sessions-file");
    g_menu_append(top, "Import Sessions\u2026", "win.import-sessions");
    g_menu_append(top, "Show Sidebar", "win.toggle-sidebar");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(top));
    g_object_unref(top);

    GMenu *tabs = g_menu_new();
    g_menu_append(tabs, "Copy", "win.copy");
    g_menu_append(tabs, "Paste", "win.paste");
    g_menu_append(tabs, "Select All", "win.select-all");
    g_menu_append(tabs, "Next Tab", "win.next-tab");
    g_menu_append(tabs, "Previous Tab", "win.prev-tab");
    g_menu_append(tabs, "Close Tab", "win.close-tab");
    g_menu_append(tabs, "Log This Session", "win.log");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(tabs));
    g_object_unref(tabs);

    GMenu *zoom = g_menu_new();
    g_menu_append(zoom, "Zoom In", "win.zoom-in");
    g_menu_append(zoom, "Zoom Out", "win.zoom-out");
    g_menu_append(zoom, "Normal Size", "win.zoom-reset");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(zoom));
    g_object_unref(zoom);

    GMenu *appearance = g_menu_new();
    g_menu_append(appearance, "Light", "win.app-variant::light");
    g_menu_append(appearance, "Dark", "win.app-variant::dark");
    GMenu *appearance_section = g_menu_new();
    g_menu_append_submenu(appearance_section, "Appearance", G_MENU_MODEL(appearance));

    // The theme list is data: one radio item per theme found in the resources.
    GMenu *theme_menu = g_menu_new();
    for (const Theme &t : themes_) {
        GMenuItem *item = g_menu_item_new(t.label.c_str(), nullptr);
        g_menu_item_set_action_and_target_value(item, "win.terminal-theme",
                                                g_variant_new_string(t.name.c_str()));
        g_menu_append_item(theme_menu, item);
        g_object_unref(item);
    }
    g_menu_append_submenu(appearance_section, "Terminal Theme", G_MENU_MODEL(theme_menu));
    g_object_unref(theme_menu);
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(appearance_section));
    g_object_unref(appearance);
    g_object_unref(appearance_section);

    GMenu *bottom = g_menu_new();
    g_menu_append(bottom, "Quit", "win.quit");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(bottom));
    g_object_unref(bottom);

    return G_MENU_MODEL(menu);  // caller owns the returned reference
}

void AppWindow::act_quit(GSimpleAction *, GVariant *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    gtk_window_close(GTK_WINDOW(self->window_));  // runs close-request, which saves
}

void AppWindow::act_reload(GSimpleAction *, GVariant *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    self->load_sessions_file();
    self->rebuild_tree(self->capture_expanded(), !self->filter_.empty());
}

// Open the sessions file in the user's default editor.
void AppWindow::act_open_sessions_file(GSimpleAction *, GVariant *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    char *uri = g_filename_to_uri(self->sessions_path_.c_str(), nullptr, nullptr);
    if (uri)
        gtk_show_uri(GTK_WINDOW(self->window_), uri, GDK_CURRENT_TIME);  // 4.8 API
    g_free(uri);
}

void AppWindow::act_toggle_sidebar(GSimpleAction *action, GVariant *value, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    const bool visible = g_variant_get_boolean(value);
    g_debug("toggle-sidebar -> %d", visible);
    g_simple_action_set_state(action, value);  // change_state must set the state itself
    gtk_widget_set_visible(self->sidebar_, visible);
    self->settings_.sidebar_visible = visible;
    self->save_settings_soon();
}

void AppWindow::act_app_variant(GSimpleAction *action, GVariant *value, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    const char *variant = g_variant_get_string(value, nullptr);
    g_debug("app-variant -> %s", variant);
    g_simple_action_set_state(action, value);
    self->set_app_variant(variant);
    self->settings_.app_variant = variant;
    self->save_settings_soon();
}

void AppWindow::set_app_variant(const std::string &variant) {
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
                 variant == "dark", nullptr);
}

// --- debounced save ----------------------------------------------------------

// Every change restarts a 500 ms timer; only the last one in a burst writes.
void AppWindow::save_settings_soon() {
    if (save_source_)
        g_source_remove(save_source_);
    save_source_ = g_timeout_add(500, on_save_timeout, this);
}

gboolean AppWindow::on_save_timeout(gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    self->save_source_ = 0;  // returning G_SOURCE_REMOVE destroys the source
    self->save_settings_now();
    return G_SOURCE_REMOVE;
}

void AppWindow::save_settings_now() {
    if (save_source_) {
        g_source_remove(save_source_);
        save_source_ = 0;
    }
    std::string err;
    if (!save_settings(config_path("settings.ini"), settings_, err))
        g_warning("settings not saved: %s", err.c_str());
    else
        g_debug("settings saved");
}

// --- popovers ----------------------------------------------------------------

Tab *AppWindow::current_tab() const {
    const int n = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_));
    if (n < 0)
        return nullptr;
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook_), n);
    return static_cast<Tab *>(g_object_get_data(G_OBJECT(page), "tab"));
}

// A GtkPopoverMenu parented to `parent`, pointing at (x, y) in `from`'s
// coordinates. The parent must outlive anything the menu can do: Close Tab
// destroys the tab page, so the terminal menu hangs off the notebook.
void AppWindow::show_popover(GMenuModel *model, GtkWidget *parent, GtkWidget *from, double x, double y) {
    // gtk_widget_compute_point exists in every GTK 4; the older
    // gtk_widget_translate_coordinates is deprecated from 4.12.
    const graphene_point_t in = GRAPHENE_POINT_INIT(static_cast<float>(x), static_cast<float>(y));
    graphene_point_t out;
    if (!gtk_widget_compute_point(from, parent, &in, &out))
        return;
    const double px = out.x, py = out.y;
    g_debug("popover: from (%.0f,%.0f) to parent (%.0f,%.0f)", x, y, px, py);

    GtkWidget *popover = gtk_popover_menu_new_from_model(model);
    gtk_widget_set_parent(popover, parent);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_set_halign(popover, GTK_ALIGN_START);
    GdkRectangle rect = {static_cast<int>(px), static_cast<int>(py), 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    g_signal_connect(popover, "closed", G_CALLBACK(on_popover_closed), this);
    gtk_popover_popup(GTK_POPOVER(popover));
}

// "closed" fires before the chosen item's action runs. Unparenting here
// would destroy the menu under its own feet, so do it on idle.
void AppWindow::on_popover_closed(GtkPopover *popover, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    g_idle_add(
        [](gpointer p) -> gboolean {
            GtkWidget *w = GTK_WIDGET(p);
            if (gtk_widget_get_parent(w))
                gtk_widget_unparent(w);
            g_object_unref(w);
            return G_SOURCE_REMOVE;
        },
        g_object_ref(popover));
    // Without this, focus lands on the notebook's tab strip when the menu
    // closes, and the next keys typed go nowhere.
    if (Tab *tab = self->current_tab())
        focus_later(tab->view);
}

void AppWindow::show_terminal_menu(Tab *tab, double x, double y) {
    // Actions work on the current tab, so make the clicked one current.
    const int n = gtk_notebook_page_num(GTK_NOTEBOOK(notebook_), tab->page);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), n);

    GMenu *menu = g_menu_new();
    GMenu *edit = g_menu_new();
    g_menu_append(edit, "Copy", "win.copy");
    g_menu_append(edit, "Paste", "win.paste");
    g_menu_append(edit, "Select All", "win.select-all");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(edit));
    GMenu *tabs = g_menu_new();
    g_menu_append(tabs, "Close Tab", "win.close-tab");
    g_menu_append(tabs, "Close Other Tabs", "win.close-other-tabs");
    g_menu_append(tabs, "Log This Session", "win.log");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(tabs));
    show_popover(G_MENU_MODEL(menu), notebook_, tab->page, x, y);
    g_object_unref(edit);
    g_object_unref(tabs);
    g_object_unref(menu);
}

void AppWindow::on_row_pressed(GtkGestureClick *gesture, int, double x, double y, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    auto *item = GTK_LIST_ITEM(g_object_get_data(G_OBJECT(gesture), "list-item"));
    auto *row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(item));
    if (!row)
        return;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    auto *si = GTKLAB_SESSION_ITEM(gtk_tree_list_row_get_item(row));
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    if (gtklab_session_item_is_folder(si)) {
        Session none;
        self->show_row_menu(widget, none, x, y);
    } else {
        self->show_row_menu(widget, gtklab_session_item_session(si), x, y);
    }
    g_object_unref(si);
}

void AppWindow::show_row_menu(GtkWidget *row, const Session &s, double x, double y) {
    GMenu *menu = g_menu_new();
    if (!s.name.empty()) {
        GMenuItem *open = g_menu_item_new("Open", nullptr);
        g_menu_item_set_action_and_target_value(open, "win.open-session",
                                                g_variant_new_string(s.name.c_str()));
        g_menu_append_item(menu, open);
        g_object_unref(open);
    }
    g_menu_append(menu, "Edit Sessions File", "win.open-sessions-file");
    // Row widgets are recycled by the list view; hang the menu off the
    // list view itself.
    show_popover(G_MENU_MODEL(menu), list_view_, row, x, y);
    g_object_unref(menu);
}

const Session *AppWindow::find_session(const std::string &name) const {
    for (const Folder &f : folders_)
        for (const Session &s : f.sessions)
            if (s.name == name)
                return &s;
    return nullptr;
}

void AppWindow::act_open_session(GSimpleAction *, GVariant *param, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    if (const Session *s = self->find_session(g_variant_get_string(param, nullptr)))
        self->add_tab(*s);
}

void AppWindow::act_copy(GSimpleAction *, GVariant *, gpointer data) {
    if (Tab *tab = static_cast<AppWindow *>(data)->current_tab())
        gtkpyte_terminal_copy_clipboard(GTKPYTE_TERMINAL(tab->view));
}

void AppWindow::act_paste(GSimpleAction *, GVariant *, gpointer data) {
    if (Tab *tab = static_cast<AppWindow *>(data)->current_tab())
        gtkpyte_terminal_paste_clipboard(GTKPYTE_TERMINAL(tab->view));
}

void AppWindow::act_select_all(GSimpleAction *, GVariant *, gpointer data) {
    if (Tab *tab = static_cast<AppWindow *>(data)->current_tab())
        gtkpyte_terminal_select_all(GTKPYTE_TERMINAL(tab->view));
}

// --- closing tabs, with a confirm dialog -----------------------------------

void AppWindow::close_tab(Tab *tab) {
    const int n = gtk_notebook_page_num(GTK_NOTEBOOK(notebook_), tab->page);
    if (n >= 0)
        gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_), n);  // deletes tab
}

void AppWindow::act_close_tab(GSimpleAction *, GVariant *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    Tab *tab = self->current_tab();
    if (!tab)
        return;
    // The dialog is asynchronous: hold the page, not the Tab pointer, in case
    // the tab is closed some other way while the dialog is up.
    self->confirm("Close " + tab->session.name + "?", "The session in this tab will end.", "Close",
                  [](AppWindow *w, gpointer page) {
                      if (auto *t = static_cast<Tab *>(g_object_get_data(G_OBJECT(page), "tab")))
                          if (gtk_widget_get_parent(GTK_WIDGET(page)))
                              w->close_tab(t);
                  },
                  tab->page);
}

// Wraps around at either end.
void AppWindow::act_next_tab(GSimpleAction *, GVariant *, gpointer data) {
    auto *nb = GTK_NOTEBOOK(static_cast<AppWindow *>(data)->notebook_);
    const int n = gtk_notebook_get_n_pages(nb);
    if (n > 0)
        gtk_notebook_set_current_page(nb, (gtk_notebook_get_current_page(nb) + 1) % n);
}

void AppWindow::act_prev_tab(GSimpleAction *, GVariant *, gpointer data) {
    auto *nb = GTK_NOTEBOOK(static_cast<AppWindow *>(data)->notebook_);
    const int n = gtk_notebook_get_n_pages(nb);
    if (n > 0)
        gtk_notebook_set_current_page(nb, (gtk_notebook_get_current_page(nb) + n - 1) % n);
}

void AppWindow::act_close_other_tabs(GSimpleAction *, GVariant *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    Tab *keep = self->current_tab();
    if (!keep)
        return;
    for (int i = gtk_notebook_get_n_pages(GTK_NOTEBOOK(self->notebook_)) - 1; i >= 0; i--) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(self->notebook_), i);
        if (page != keep->page)
            gtk_notebook_remove_page(GTK_NOTEBOOK(self->notebook_), i);
    }
}

namespace {
struct ConfirmData {
    AppWindow *window;
    void (*on_accept)(AppWindow *, gpointer);
    GObject *object;  // kept alive until the dialog answers
};
}  // namespace

// A modal question. Nothing blocks: the answer arrives in "response".
void AppWindow::confirm(const std::string &title, const std::string &detail, const char *accept_label,
                        void (*on_accept)(AppWindow *, gpointer), gpointer data) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s",
                                               title.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", detail.c_str());
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    GtkWidget *accept = gtk_dialog_add_button(GTK_DIALOG(dialog), accept_label, GTK_RESPONSE_ACCEPT);
    gtk_widget_add_css_class(accept, "destructive-action");
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    auto *cd = new ConfirmData{this, on_accept, G_OBJECT(g_object_ref(data))};
    g_signal_connect(dialog, "response",
                     G_CALLBACK(+[](GtkDialog *d, int response, gpointer p) {
                         auto *c = static_cast<ConfirmData *>(p);
                         g_debug("confirm response %d", response);
                         if (response == GTK_RESPONSE_ACCEPT)
                             c->on_accept(c->window, c->object);
                         g_object_unref(c->object);
                         delete c;
                         gtk_window_destroy(GTK_WINDOW(d));
                     }),
                     cd);
    gtk_window_present(GTK_WINDOW(dialog));
}

void AppWindow::message(const std::string &title, const std::string &detail) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s",
                                               title.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", detail.c_str());
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), nullptr);
    gtk_window_present(GTK_WINDOW(dialog));
}

// --- import ------------------------------------------------------------------

// GtkFileChooserNative is the 4.8 API; GtkFileDialog arrived in 4.10.
void AppWindow::act_import_file(GSimpleAction *, GVariant *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
        "Import Sessions", GTK_WINDOW(self->window_), GTK_FILE_CHOOSER_ACTION_OPEN, "Import", "Cancel");
    gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(chooser), TRUE);
    g_signal_connect(chooser, "response", G_CALLBACK(on_import_response), self);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(chooser));
    // We hold the only reference; on_import_response drops it.
}

void AppWindow::on_import_response(GtkNativeDialog *dialog, int response, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        char *path = g_file_get_path(file);
        std::string err;
        load_sessions(path ? path : "", err);  // validate before replacing anything
        if (!err.empty()) {
            self->message("Not imported", err);
        } else {
            char *contents = nullptr;
            gsize len = 0;
            GError *gerr = nullptr;
            if (g_file_get_contents(path, &contents, &len, &gerr) &&
                g_file_set_contents(self->sessions_path_.c_str(), contents, static_cast<gssize>(len), &gerr)) {
                self->load_sessions_file();
                self->rebuild_tree(self->capture_expanded(), !self->filter_.empty());
            } else {
                self->message("Not imported", gerr ? gerr->message : "unknown error");
            }
            g_clear_error(&gerr);
            g_free(contents);
        }
        g_free(path);
        g_object_unref(file);
    }
    g_object_unref(dialog);
}

// --- font zoom ---------------------------------------------------------------

void AppWindow::apply_font(Tab *tab) {
    const std::string font = "Monospace " + std::to_string(settings_.font_size);
    gtkpyte_terminal_set_font(GTKPYTE_TERMINAL(tab->view), font.c_str());
}

// One font size for every tab, saved with the debounced save.
void AppWindow::set_font_size(int size) {
    size = CLAMP(size, 6, 48);
    if (size == settings_.font_size)
        return;
    settings_.font_size = size;
    g_debug("font size %d", size);
    for (int i = 0; i < gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)); i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook_), i);
        if (auto *tab = static_cast<Tab *>(g_object_get_data(G_OBJECT(page), "tab")))
            apply_font(tab);
    }
    save_settings_soon();
}

void AppWindow::act_zoom(GSimpleAction *action, GVariant *, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    const char *name = g_action_get_name(G_ACTION(action));
    if (g_str_equal(name, "zoom-in"))
        self->set_font_size(self->settings_.font_size + 1);
    else if (g_str_equal(name, "zoom-out"))
        self->set_font_size(self->settings_.font_size - 1);
    else
        self->set_font_size(Settings{}.font_size);
}

// --- session logging -----------------------------------------------------------

// The check mark follows the current tab.
void AppWindow::sync_log_action() {
    Tab *tab = current_tab();
    GAction *a = g_action_map_lookup_action(G_ACTION_MAP(window_), "log");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(a), tab != nullptr);
    g_simple_action_set_state(G_SIMPLE_ACTION(a), g_variant_new_boolean(tab && tab->log));
}

void AppWindow::act_log(GSimpleAction *action, GVariant *value, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    Tab *tab = self->current_tab();
    if (!tab)
        return;
    if (g_variant_get_boolean(value)) {
        tab->log = std::make_unique<SessionLog>(tab->session.name);
        if (!tab->log->ok()) {
            self->message("Logging failed", "Could not open " + tab->log->path());
            tab->log.reset();
        } else {
            g_debug("logging %s to %s", tab->session.name.c_str(), tab->log->path().c_str());
        }
    } else {
        tab->log.reset();  // writes the footer and closes the file
    }
    gtk_widget_set_visible(tab->log_dot, tab->log != nullptr);
    g_simple_action_set_state(action, g_variant_new_boolean(tab->log != nullptr));
}

void AppWindow::on_switch_page(GtkNotebook *, GtkWidget *, guint, gpointer data) {
    // switch-page runs before the notebook's current page changes; sync on idle.
    g_idle_add(
        [](gpointer p) -> gboolean {
            static_cast<AppWindow *>(p)->sync_log_action();
            return G_SOURCE_REMOVE;
        },
        data);
}

// --- resources: CSS, icons, themes -----------------------------------------------

void AppWindow::install_style() {
    GdkDisplay *display = gdk_display_get_default();

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/io/github/gtklab/style.css");
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(css),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    // Icons under <path>/scalable/actions/ become part of the icon theme,
    // so gtk_image_new_from_icon_name finds them with no icons installed.
    gtk_icon_theme_add_resource_path(gtk_icon_theme_get_for_display(display), "/io/github/gtklab/icons");
}

const Theme *AppWindow::find_theme(const std::string &name) const {
    for (const Theme &t : themes_)
        if (t.name == name)
            return &t;
    return themes_.empty() ? nullptr : &themes_.front();
}

void AppWindow::apply_terminal_theme(Tab *tab) {
    if (const Theme *t = find_theme(settings_.terminal_theme))
        apply_theme(GTKPYTE_TERMINAL(tab->view), *t);
}

void AppWindow::act_terminal_theme(GSimpleAction *action, GVariant *value, gpointer data) {
    auto *self = static_cast<AppWindow *>(data);
    g_simple_action_set_state(action, value);
    self->settings_.terminal_theme = g_variant_get_string(value, nullptr);
    for (int i = 0; i < gtk_notebook_get_n_pages(GTK_NOTEBOOK(self->notebook_)); i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(self->notebook_), i);
        if (auto *tab = static_cast<Tab *>(g_object_get_data(G_OBJECT(page), "tab")))
            self->apply_terminal_theme(tab);
    }
    self->save_settings_soon();
}
