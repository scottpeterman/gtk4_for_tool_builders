#include <gtk/gtk.h>

static gboolean on_close_request(GtkWindow *win, gpointer) {
    int w = 0, h = 0;
    gtk_window_get_default_size(win, &w, &h);
    g_print("closing at %dx%d\n", w, h);
    return FALSE;
}

// Temporary until Chapter 7 turns this into an action.
static void on_flip_clicked(GtkButton *, gpointer user_data) {
    GtkStack *stack = GTK_STACK(user_data);
    const char *now = gtk_stack_get_visible_child_name(stack);
    gtk_stack_set_visible_child_name(stack, g_strcmp0(now, "empty") == 0 ? "tabs" : "empty");
}

static GtkWidget *build_sidebar() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);
    gtk_widget_set_margin_start(box, 6);
    gtk_widget_set_margin_end(box, 6);

    GtkWidget *search = gtk_search_entry_new();
    gtk_box_append(GTK_BOX(box), search);

    GtkWidget *placeholder = gtk_label_new("Sessions go here (Chapter 5)");
    gtk_widget_add_css_class(placeholder, "dim-label");
    gtk_widget_set_valign(placeholder, GTK_ALIGN_START);

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), placeholder);
    gtk_box_append(GTK_BOX(box), scroller);
    return box;
}

static GtkWidget *build_content(GtkWidget **stack_out) {
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_hhomogeneous(GTK_STACK(stack), FALSE);  // measure only the visible page
    gtk_stack_set_vhomogeneous(GTK_STACK(stack), FALSE);

    GtkWidget *hint = gtk_label_new("No tabs open");
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_stack_add_named(GTK_STACK(stack), hint, "empty");

    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gtk_label_new("A page"),
                             gtk_label_new("Tab 1"));
    gtk_stack_add_named(GTK_STACK(stack), notebook, "tabs");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "empty");
    *stack_out = stack;
    return stack;
}

static void on_activate(GtkApplication *app, gpointer) {
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "gtklab");
    gtk_window_set_default_size(GTK_WINDOW(win), 900, 600);
    g_signal_connect(win, "close-request", G_CALLBACK(on_close_request), nullptr);

    GtkWidget *stack = nullptr;
    GtkWidget *content = build_content(&stack);

    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *flip = gtk_button_new_with_label("Flip");
    g_signal_connect(flip, "clicked", G_CALLBACK(on_flip_clicked), stack);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), flip);
    gtk_window_set_titlebar(GTK_WINDOW(win), header);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned), build_sidebar());
    gtk_paned_set_end_child(GTK_PANED(paned), content);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 260);

    gtk_window_set_child(GTK_WINDOW(win), paned);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("io.github.gtklab", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
