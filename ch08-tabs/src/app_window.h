// AppWindow: the window and everything it owns.
#pragma once

#include <gtk/gtk.h>

#include <set>
#include <string>
#include <vector>

#include "core/session.h"
#include "core/settings.h"
#include "tab.h"

class AppWindow {
public:
    explicit AppWindow(GtkApplication *app);
    ~AppWindow();

    AppWindow(const AppWindow &) = delete;
    AppWindow &operator=(const AppWindow &) = delete;

    GtkWindow *window() const { return GTK_WINDOW(window_); }

private:
    void build_ui();
    GtkWidget *build_sidebar();
    void load_sessions_file();
    void rebuild_tree(const std::set<std::string> &expand, bool expand_all);
    std::set<std::string> capture_expanded() const;
    void activate_session(const Session &s);
    Tab *add_tab(const Session &s);
    void install_actions(GtkApplication *app);
    GMenuModel *build_menu();
    void set_app_variant(const std::string &variant);
    void save_settings_soon();
    void save_settings_now();

    static GListModel *create_child_model(gpointer item, gpointer self);
    static gboolean on_close_request(GtkWindow *win, gpointer self);
    static void act_quit(GSimpleAction *, GVariant *, gpointer self);
    static void act_reload(GSimpleAction *, GVariant *, gpointer self);
    static void act_open_sessions_file(GSimpleAction *, GVariant *, gpointer self);
    static void act_toggle_sidebar(GSimpleAction *action, GVariant *value, gpointer self);
    static void act_app_variant(GSimpleAction *action, GVariant *value, gpointer self);
    static gboolean on_save_timeout(gpointer self);
    static void on_tab_close_clicked(GtkButton *, gpointer page);
    static void on_page_added(GtkNotebook *, GtkWidget *, guint, gpointer self);
    static void on_page_removed(GtkNotebook *, GtkWidget *, guint, gpointer self);
    static void on_search_changed(GtkSearchEntry *, gpointer self);
    static void on_row_activated(GtkListView *, guint position, gpointer self);
    static void on_factory_setup(GtkSignalListItemFactory *, GtkListItem *item, gpointer self);
    static void on_factory_bind(GtkSignalListItemFactory *, GtkListItem *item, gpointer self);
    static void on_factory_unbind(GtkSignalListItemFactory *, GtkListItem *item, gpointer self);

    GtkWidget *window_ = nullptr;
    GtkWidget *stack_ = nullptr;
    GtkWidget *notebook_ = nullptr;
    GtkWidget *paned_ = nullptr;
    GtkWidget *sidebar_ = nullptr;
    GtkWidget *search_ = nullptr;
    GtkWidget *error_label_ = nullptr;
    GtkWidget *list_view_ = nullptr;
    GListStore *root_ = nullptr;             // owned: folder items
    GtkTreeListModel *tree_model_ = nullptr; // owned

    Settings settings_;
    std::string sessions_path_;
    std::vector<Folder> folders_;
    std::string filter_;
    std::set<std::string> user_expanded_;  // folders open before a filter began
    guint save_source_ = 0;                // pending debounced save, 0 if none
};
