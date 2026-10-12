// Lab 8: tabs that own their state. Each notebook page owns its Session
// through object data, so the notebook is the only list of tabs. Commands
// resolve the current page when they run; nothing keeps a "current session"
// pointer that could go stale.
#include <gtk/gtk.h>
#include <string>

struct Session {
    int id;
    int counter = 0;
    GtkWidget *label = nullptr;  // borrowed: the page owns it
    explicit Session(int number) : id(number) { g_print("CREATE session %d\n", id); }
    ~Session() { g_print("DESTROY session %d, count=%d\n", id, counter); }
};

struct WindowState {
    GtkWidget *notebook = nullptr;  // borrowed: the window owns it
    GtkWidget *status = nullptr;
    int next_id = 1;
};

static Session *session_for(GtkWidget *page) {
    return page ? static_cast<Session *>(g_object_get_data(G_OBJECT(page), "session")) : nullptr;
}

// Resolved at the moment a command runs, from the notebook itself.
static Session *active_session(WindowState *state) {
    GtkNotebook *nb = GTK_NOTEBOOK(state->notebook);
    int index = gtk_notebook_get_current_page(nb);
    return index < 0 ? nullptr : session_for(gtk_notebook_get_nth_page(nb, index));
}

static void show_status(GtkWidget *status, GtkNotebook *nb, Session *session) {
    int tabs = gtk_notebook_get_n_pages(nb);
    std::string message = session ? "Active session " + std::to_string(session->id) +
                                        " | counter=" + std::to_string(session->counter) +
                                        " | total tabs=" + std::to_string(tabs)
                                  : "No active tab";
    gtk_label_set_text(GTK_LABEL(status), message.c_str());
}

static void add_tab(WindowState *state) {
    auto *session = new Session(state->next_id++);
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(page, 28);
    gtk_widget_set_margin_start(page, 28);
    session->label = gtk_label_new("Counter: 0");
    gtk_box_append(GTK_BOX(page), session->label);

    // The page owns the session: it is deleted when the page is destroyed,
    // whether the tab was closed or the window went away.
    g_object_set_data_full(G_OBJECT(page), "session", session,
                           [](gpointer p) { delete static_cast<Session *>(p); });

    std::string name = "Session " + std::to_string(session->id);
    int index = gtk_notebook_append_page(GTK_NOTEBOOK(state->notebook), page,
                                         gtk_label_new(name.c_str()));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), index);
}

static void increment_active(WindowState *state) {
    Session *session = active_session(state);
    if (!session)
        return;
    ++session->counter;
    std::string text = "Counter: " + std::to_string(session->counter);
    gtk_label_set_text(GTK_LABEL(session->label), text.c_str());
    show_status(state->status, GTK_NOTEBOOK(state->notebook), session);
}

static void close_active(WindowState *state) {
    int index = gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook));
    if (index >= 0)
        gtk_notebook_remove_page(GTK_NOTEBOOK(state->notebook), index);  // destroys page and session
}

static void on_new(GtkButton *, gpointer data) { add_tab(static_cast<WindowState *>(data)); }
static void on_increment(GtkButton *, gpointer data) {
    increment_active(static_cast<WindowState *>(data));
}
static void on_close(GtkButton *, gpointer data) { close_active(static_cast<WindowState *>(data)); }

// The notebook's handlers get the status label, not the window state, and
// are connected with g_signal_connect_object (Chapter 2): when the label is
// destroyed, they are disconnected. So whichever of the label and the
// notebook GTK tears down first at quit, no handler touches a dead widget.
// switch-page passes the page being switched to.
static void on_switch(GtkNotebook *nb, GtkWidget *page, guint, gpointer status) {
    show_status(GTK_WIDGET(status), nb, session_for(page));
}
static void on_page_removed(GtkNotebook *nb, GtkWidget *, guint, gpointer status) {
    if (gtk_notebook_get_n_pages(nb) == 0)
        show_status(GTK_WIDGET(status), nb, nullptr);
}

static void activate(GtkApplication *app, gpointer) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Tab Scope Laboratory");
    gtk_window_set_default_size(GTK_WINDOW(window), 660, 380);

    auto *state = new WindowState;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_window_set_child(GTK_WINDOW(window), root);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(root), toolbar);
    GtkWidget *new_button = gtk_button_new_with_label("New tab");
    GtkWidget *increment_button = gtk_button_new_with_label("Increment active");
    GtkWidget *close_button = gtk_button_new_with_label("Close active");
    gtk_box_append(GTK_BOX(toolbar), new_button);
    gtk_box_append(GTK_BOX(toolbar), increment_button);
    gtk_box_append(GTK_BOX(toolbar), close_button);

    state->notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(state->notebook, TRUE);
    gtk_box_append(GTK_BOX(root), state->notebook);
    state->status = gtk_label_new("No active tab");
    gtk_box_append(GTK_BOX(root), state->status);

    g_signal_connect(new_button, "clicked", G_CALLBACK(on_new), state);
    g_signal_connect(increment_button, "clicked", G_CALLBACK(on_increment), state);
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_close), state);
    g_signal_connect_object(state->notebook, "switch-page", G_CALLBACK(on_switch),
                            state->status, G_CONNECT_DEFAULT);
    g_signal_connect_object(state->notebook, "page-removed", G_CALLBACK(on_page_removed),
                            state->status, G_CONNECT_DEFAULT);

    // The window owns the state. Only the buttons' handlers hold it, and they
    // cannot run once the window is being destroyed.
    g_object_set_data_full(G_OBJECT(window), "state", state,
                           [](gpointer p) { delete static_cast<WindowState *>(p); });

    add_tab(state);
    add_tab(state);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab.lab08", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}
