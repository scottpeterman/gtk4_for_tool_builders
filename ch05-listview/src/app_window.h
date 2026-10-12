// AppWindow: the window and everything it owns. From Chapter 5 on, the
// window's state lives in this struct instead of globals.
#pragma once

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "core/session.h"
#include "core/settings.h"

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
    void fill_list();

    static gboolean on_close_request(GtkWindow *win, gpointer self);
    static void on_flip_clicked(GtkButton *, gpointer self);
    static void on_factory_setup(GtkSignalListItemFactory *, GtkListItem *item, gpointer self);
    static void on_factory_bind(GtkSignalListItemFactory *, GtkListItem *item, gpointer self);
    static void on_factory_unbind(GtkSignalListItemFactory *, GtkListItem *item, gpointer self);

    GtkWidget *window_ = nullptr;
    GtkWidget *stack_ = nullptr;
    GtkWidget *notebook_ = nullptr;
    GtkWidget *paned_ = nullptr;
    GtkWidget *list_view_ = nullptr;
    GListStore *store_ = nullptr;  // owned: GtklabSessionItem

    Settings settings_;
    std::string sessions_path_;
    std::vector<Folder> folders_;
};
