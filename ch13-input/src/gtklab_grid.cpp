#include "gtklab_grid.h"

#include <algorithm>
#include <deque>
#include <new>
#include <string>
#include <vector>

// Colours are fixed until Chapter 15 makes them theme properties.
static const GdkRGBA kBackground = {0.12f, 0.12f, 0.14f, 1.0f};
static const GdkRGBA kForeground = {0.85f, 0.86f, 0.88f, 1.0f};
static const GdkRGBA kCursor = {0.55f, 0.75f, 1.0f, 1.0f};
static const GdkRGBA kSelection = {0.30f, 0.45f, 0.75f, 0.55f};

static const int kMaxScrollback = 5000;  // lines kept above the screen

using Line = std::vector<gunichar>;

struct CellPos {
    long line;  // absolute line index in `lines`
    int col;
};

struct _GtklabGrid {
    GtkWidget parent_instance;

    // C++ members: built with placement new in init, destroyed in finalize.
    std::string font;
    std::deque<Line> lines;  // scrollback followed by the screen (last `rows` lines)
    std::string pending;     // incomplete UTF-8 or escape sequence

    PangoFontDescription *font_desc;  // owned
    PangoAttrList *attrs;             // owned
    int cell_width;
    int cell_height;
    int cols;
    int rows;
    long cursor_line;  // absolute
    int cursor_col;

    // GtkScrollable
    GtkAdjustment *hadjustment;  // owned (we keep it only because the interface requires it)
    GtkAdjustment *vadjustment;  // owned
    GtkScrollablePolicy hscroll_policy;
    GtkScrollablePolicy vscroll_policy;

    // Selection, in absolute cells. Active while has_selection.
    gboolean has_selection;
    CellPos sel_anchor;
    CellPos sel_head;
    double drag_x;
    double drag_y;
};

enum {
    PROP_0,
    PROP_FONT,
    N_PROPS,
    // GtkScrollable's properties, overridden rather than installed
    PROP_HADJUSTMENT = N_PROPS,
    PROP_VADJUSTMENT,
    PROP_HSCROLL_POLICY,
    PROP_VSCROLL_POLICY,
};
static GParamSpec *props[N_PROPS];

enum { SIGNAL_RESIZED, SIGNAL_COMMIT, N_SIGNALS };
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE_WITH_CODE(GtklabGrid, gtklab_grid, GTK_TYPE_WIDGET,
                              G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, nullptr))

static long screen_start(GtklabGrid *self) {
    return static_cast<long>(self->lines.size()) - self->rows;
}

// --- scrolling -------------------------------------------------------------------

// The first line shown: the adjustment's value when we have one.
static long top_line(GtklabGrid *self) {
    if (!self->vadjustment)
        return screen_start(self);
    return std::clamp(static_cast<long>(gtk_adjustment_get_value(self->vadjustment)), 0L,
                      screen_start(self));
}

static gboolean at_bottom(GtklabGrid *self) {
    return !self->vadjustment || top_line(self) >= screen_start(self);
}

// Keep the adjustment's range in step with the line count. Units are lines:
// upper = every line, page_size = one screen.
static void configure_adjustment(GtklabGrid *self, gboolean follow) {
    if (!self->vadjustment)
        return;
    const double upper = static_cast<double>(self->lines.size());
    const double value = follow ? static_cast<double>(screen_start(self))
                                : std::min(gtk_adjustment_get_value(self->vadjustment),
                                           static_cast<double>(screen_start(self)));
    gtk_adjustment_configure(self->vadjustment, value, 0.0, upper, 1.0, self->rows, self->rows);
}

static void on_vadjustment_changed(GtkAdjustment *, gpointer data) {
    gtk_widget_queue_draw(GTK_WIDGET(data));
}

static void set_vadjustment(GtklabGrid *self, GtkAdjustment *adj) {
    if (self->vadjustment == adj)
        return;
    if (self->vadjustment) {
        g_signal_handlers_disconnect_by_data(self->vadjustment, self);
        g_object_unref(self->vadjustment);
    }
    if (!adj)
        adj = gtk_adjustment_new(0, 0, 0, 0, 0, 0);
    self->vadjustment = GTK_ADJUSTMENT(g_object_ref_sink(adj));
    g_signal_connect(adj, "value-changed", G_CALLBACK(on_vadjustment_changed), self);
    configure_adjustment(self, TRUE);
}

static void set_hadjustment(GtklabGrid *self, GtkAdjustment *adj) {
    if (self->hadjustment == adj)
        return;
    g_clear_object(&self->hadjustment);
    if (!adj)
        adj = gtk_adjustment_new(0, 0, 0, 0, 0, 0);
    self->hadjustment = GTK_ADJUSTMENT(g_object_ref_sink(adj));  // never scrolls horizontally
}

// --- cell size ---------------------------------------------------------------------

// Base attributes for every layout: the font, and no ligatures. With
// ligatures on, "->" or "==" can become one glyph wider or narrower than two
// cells, and every character after it lands off the grid.
static PangoAttrList *base_attrs(GtklabGrid *self) {
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_font_desc_new(self->font_desc));
    pango_attr_list_insert(attrs, pango_attr_font_features_new("liga 0, calt 0"));
    return attrs;
}

static void measure_cells(GtklabGrid *self) {
    PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), "M");
    pango_layout_set_attributes(layout, self->attrs);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(layout, &w, &h);
    g_object_unref(layout);
    self->cell_width = std::max(w, 1);
    self->cell_height = std::max(h, 1);
}

// --- the cell model ----------------------------------------------------------------

static void trim_scrollback(GtklabGrid *self) {
    while (static_cast<long>(self->lines.size()) > self->rows + kMaxScrollback) {
        self->lines.pop_front();
        self->cursor_line--;
        self->sel_anchor.line--;
        self->sel_head.line--;
    }
    if (self->has_selection && std::min(self->sel_anchor.line, self->sel_head.line) < 0)
        self->has_selection = FALSE;
}

static void resize_screen(GtklabGrid *self, int cols, int rows) {
    for (Line &line : self->lines)
        line.resize(static_cast<size_t>(cols), ' ');
    self->cols = cols;
    self->rows = rows;
    // Shrinking: drop blank lines below the cursor before pushing text
    // into scrollback.
    while (screen_start(self) > self->cursor_line &&
           static_cast<long>(self->lines.size()) - 1 > self->cursor_line &&
           std::all_of(self->lines.back().begin(), self->lines.back().end(),
                       [](gunichar c) { return c == ' '; }))
        self->lines.pop_back();
    // Growing: blank lines below.
    while (static_cast<long>(self->lines.size()) < rows)
        self->lines.push_back(Line(static_cast<size_t>(cols), ' '));
    self->cursor_line = std::clamp(self->cursor_line, screen_start(self),
                                   static_cast<long>(self->lines.size()) - 1);
    self->cursor_col = std::clamp(self->cursor_col, 0, cols - 1);
    trim_scrollback(self);
}

static void update_grid(GtklabGrid *self, int width, int height) {
    const int cols = std::max(width / self->cell_width, 1);
    const int rows = std::max(height / self->cell_height, 1);
    if (cols == self->cols && rows == self->rows)
        return;
    const gboolean follow = at_bottom(self);
    resize_screen(self, cols, rows);
    configure_adjustment(self, follow);
    g_debug("update_grid: %dx%d px -> %d cols x %d rows", width, height, cols, rows);
    g_signal_emit(self, signals[SIGNAL_RESIZED], 0, cols, rows);
}

static void newline(GtklabGrid *self) {
    self->cursor_line++;
    if (self->cursor_line >= static_cast<long>(self->lines.size())) {
        self->lines.push_back(Line(static_cast<size_t>(self->cols), ' '));
        trim_scrollback(self);
    }
}

static void put_char(GtklabGrid *self, gunichar c) {
    if (self->cursor_col >= self->cols) {  // wrap
        self->cursor_col = 0;
        newline(self);
    }
    self->lines[static_cast<size_t>(self->cursor_line)][static_cast<size_t>(self->cursor_col)] = c;
    self->cursor_col++;
}

void gtklab_grid_put_text(GtklabGrid *self, int row, int col, const char *utf8) {
    g_return_if_fail(GTKLAB_IS_GRID(self));
    if (row < 0 || row >= self->rows)
        return;
    Line &line = self->lines[static_cast<size_t>(screen_start(self) + row)];
    for (const char *p = utf8; *p && col < self->cols; p = g_utf8_next_char(p), col++)
        if (col >= 0)
            line[static_cast<size_t>(col)] = g_utf8_get_char(p);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static size_t escape_length(const std::string &s, size_t i) {
    if (i + 1 >= s.size())
        return 0;
    const char kind = s[i + 1];
    if (kind == '[') {
        for (size_t j = i + 2; j < s.size(); j++)
            if (s[j] >= 0x40 && s[j] <= 0x7e)
                return j - i + 1;
        return 0;
    }
    if (kind == ']') {
        for (size_t j = i + 2; j < s.size(); j++) {
            if (s[j] == '\a')
                return j - i + 1;
            if (s[j] == '\x1b' && j + 1 < s.size() && s[j + 1] == '\\')
                return j - i + 2;
        }
        return 0;
    }
    return 2;
}

void gtklab_grid_feed(GtklabGrid *self, const char *data, gsize len) {
    g_return_if_fail(GTKLAB_IS_GRID(self));
    const gboolean follow = at_bottom(self);
    std::string &buf = self->pending;
    buf.append(data, len);

    size_t i = 0;
    while (i < buf.size()) {
        const unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c == 0x1b) {
            const size_t n = escape_length(buf, i);
            if (n == 0)
                break;
            i += n;
        } else if (c == '\r') {
            self->cursor_col = 0;
            i++;
        } else if (c == '\n') {
            newline(self);
            i++;
        } else if (c == '\b') {
            self->cursor_col = std::max(0, self->cursor_col - 1);
            i++;
        } else if (c == '\t') {
            const int next = std::min((self->cursor_col / 8 + 1) * 8, self->cols - 1);
            while (self->cursor_col < next)
                put_char(self, ' ');
            i++;
        } else if (c < 0x20 || c == 0x7f) {
            i++;
        } else {
            const gunichar u = g_utf8_get_char_validated(buf.data() + i, static_cast<gssize>(buf.size() - i));
            if (u == static_cast<gunichar>(-2))
                break;
            if (u == static_cast<gunichar>(-1)) {
                put_char(self, 0xfffd);
                i++;
                continue;
            }
            put_char(self, u);
            i = static_cast<size_t>(g_utf8_next_char(buf.data() + i) - buf.data());
        }
    }
    buf.erase(0, i);
    // New lines change the scroll range. Follow the output only if the user
    // was already at the bottom; otherwise leave their view where it is.
    configure_adjustment(self, follow);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

// --- selection ---------------------------------------------------------------------

static CellPos cell_at(GtklabGrid *self, double x, double y) {
    const long line = top_line(self) + static_cast<long>(y) / self->cell_height;
    const int col = static_cast<int>((x + self->cell_width / 2.0) / self->cell_width);
    return {std::clamp(line, 0L, static_cast<long>(self->lines.size()) - 1), std::clamp(col, 0, self->cols)};
}

// Selection bounds in reading order; end is exclusive.
static void selection_bounds(GtklabGrid *self, CellPos *start, CellPos *end) {
    const CellPos &a = self->sel_anchor;
    const CellPos &b = self->sel_head;
    const bool a_first = a.line < b.line || (a.line == b.line && a.col <= b.col);
    *start = a_first ? a : b;
    *end = a_first ? b : a;
}

gboolean gtklab_grid_has_selection(GtklabGrid *self) {
    return self->has_selection;
}

char *gtklab_grid_get_selected_text(GtklabGrid *self) {
    if (!self->has_selection)
        return nullptr;
    CellPos start, end;
    selection_bounds(self, &start, &end);
    std::string out;
    for (long l = start.line; l <= end.line; l++) {
        const Line &line = self->lines[static_cast<size_t>(l)];
        const int from = (l == start.line) ? start.col : 0;
        const int to = (l == end.line) ? end.col : self->cols;
        std::string text;
        for (int c = from; c < to; c++) {
            char utf8[6];
            text.append(utf8, static_cast<size_t>(g_unichar_to_utf8(line[static_cast<size_t>(c)], utf8)));
        }
        while (!text.empty() && text.back() == ' ')  // trailing blanks are padding
            text.pop_back();
        out += text;
        if (l != end.line)
            out += '\n';
    }
    return g_strdup(out.c_str());
}

void gtklab_grid_select_all(GtklabGrid *self) {
    self->has_selection = TRUE;
    self->sel_anchor = {0, 0};
    self->sel_head = {static_cast<long>(self->lines.size()) - 1, self->cols};
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void gtklab_grid_copy_clipboard(GtklabGrid *self) {
    char *text = gtklab_grid_get_selected_text(self);
    if (text)
        gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(self)), text);
    g_free(text);
}

static void emit_commit(GtklabGrid *self, const char *data, gsize len) {
    GBytes *bytes = g_bytes_new(data, len);
    g_signal_emit(self, signals[SIGNAL_COMMIT], 0, bytes);
    g_bytes_unref(bytes);
}

static void on_paste_ready(GObject *source, GAsyncResult *result, gpointer data) {
    auto *self = GTKLAB_GRID(data);  // we hold a reference: see paste_clipboard
    char *text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), result, nullptr);
    if (text) {
        std::string s = text;
        std::replace(s.begin(), s.end(), '\n', '\r');  // Enter sends \r
        emit_commit(self, s.data(), s.size());
        g_free(text);
    }
    g_object_unref(self);
}

void gtklab_grid_paste_clipboard(GtklabGrid *self) {
    // Asynchronous: the grid could be destroyed before the text arrives, so
    // the callback keeps it alive with a reference.
    gdk_clipboard_read_text_async(gtk_widget_get_clipboard(GTK_WIDGET(self)), nullptr, on_paste_ready,
                                  g_object_ref(self));
}

static void on_drag_begin(GtkGestureDrag *, double x, double y, gpointer data) {
    auto *self = GTKLAB_GRID(data);
    gtk_widget_grab_focus(GTK_WIDGET(self));
    self->drag_x = x;
    self->drag_y = y;
    self->sel_anchor = self->sel_head = cell_at(self, x, y);
    self->has_selection = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void on_drag_update(GtkGestureDrag *, double dx, double dy, gpointer data) {
    auto *self = GTKLAB_GRID(data);
    self->sel_head = cell_at(self, self->drag_x + dx, self->drag_y + dy);
    self->has_selection = self->sel_head.line != self->sel_anchor.line || self->sel_head.col != self->sel_anchor.col;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

// --- keys ----------------------------------------------------------------------------

// Keys to bytes for the child. Returning FALSE lets the key go on to other
// handlers. The app's own shortcuts never reach here: the window's
// capture-phase controller takes them first.
static gboolean on_key_pressed(GtkEventControllerKey *, guint keyval, guint, GdkModifierType state,
                               gpointer data) {
    auto *self = GTKLAB_GRID(data);
    std::string out;
    const bool ctrl = state & GDK_CONTROL_MASK;
    const bool shift = state & GDK_SHIFT_MASK;

    switch (keyval) {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter: out = "\r"; break;
    case GDK_KEY_BackSpace: out = "\x7f"; break;
    case GDK_KEY_Tab: out = "\t"; break;
    case GDK_KEY_Escape: out = "\x1b"; break;
    case GDK_KEY_Up: out = "\x1b[A"; break;
    case GDK_KEY_Down: out = "\x1b[B"; break;
    case GDK_KEY_Right: out = "\x1b[C"; break;
    case GDK_KEY_Left: out = "\x1b[D"; break;
    case GDK_KEY_Home: out = "\x1b[H"; break;
    case GDK_KEY_End: out = "\x1b[F"; break;
    case GDK_KEY_Delete: out = "\x1b[3~"; break;
    default:
        if (ctrl && !shift && keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) {
            out = std::string(1, static_cast<char>(keyval - GDK_KEY_a + 1));  // Ctrl+C = 0x03
        } else if (ctrl) {
            return FALSE;  // not ours
        } else {
            const gunichar u = gdk_keyval_to_unicode(keyval);
            if (u == 0)
                return FALSE;  // a modifier on its own, a function key...
            char utf8[6];
            out.assign(utf8, static_cast<size_t>(g_unichar_to_utf8(u, utf8)));
        }
    }
    g_debug("grid key: %zu bytes", out.size());
    // Typing jumps back to the bottom, like every terminal.
    if (self->vadjustment)
        gtk_adjustment_set_value(self->vadjustment, static_cast<double>(screen_start(self)));
    emit_commit(self, out.data(), out.size());
    return TRUE;
}

// --- GtkWidget virtual functions -----------------------------------------------------

static void gtklab_grid_measure(GtkWidget *widget, GtkOrientation orientation, int,
                                int *minimum, int *natural, int *, int *) {
    auto *self = GTKLAB_GRID(widget);
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = self->cell_width * 20;
        *natural = self->cell_width * 80;
    } else {
        *minimum = self->cell_height * 5;
        *natural = self->cell_height * 24;
    }
}

static void gtklab_grid_size_allocate(GtkWidget *widget, int width, int height, int) {
    update_grid(GTKLAB_GRID(widget), width, height);
}

static void paint_row(GtklabGrid *self, GtkSnapshot *snapshot, long line, int y) {
    std::string text;
    for (gunichar c : self->lines[static_cast<size_t>(line)]) {
        char utf8[6];
        text.append(utf8, static_cast<size_t>(g_unichar_to_utf8(c, utf8)));
    }
    PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), text.c_str());
    pango_layout_set_attributes(layout, self->attrs);
    gtk_snapshot_save(snapshot);
    graphene_point_t at = GRAPHENE_POINT_INIT(0.f, static_cast<float>(y));
    gtk_snapshot_translate(snapshot, &at);
    gtk_snapshot_append_layout(snapshot, layout, &kForeground);
    gtk_snapshot_restore(snapshot);
    g_object_unref(layout);
}

static void gtklab_grid_snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
    auto *self = GTKLAB_GRID(widget);
    const float w = static_cast<float>(gtk_widget_get_width(widget));
    const float h = static_cast<float>(gtk_widget_get_height(widget));
    const float cw = static_cast<float>(self->cell_width);
    const float ch = static_cast<float>(self->cell_height);

    graphene_rect_t all = GRAPHENE_RECT_INIT(0.f, 0.f, w, h);
    gtk_snapshot_append_color(snapshot, &kBackground, &all);

    const long top = top_line(self);
    const long last = std::min(top + self->rows, static_cast<long>(self->lines.size()));

    // Selection under the text.
    if (self->has_selection) {
        CellPos start, end;
        selection_bounds(self, &start, &end);
        for (long l = std::max(start.line, top); l <= std::min(end.line, last - 1); l++) {
            const int from = (l == start.line) ? start.col : 0;
            const int to = (l == end.line) ? end.col : self->cols;
            graphene_rect_t r = GRAPHENE_RECT_INIT(from * cw, (l - top) * ch, (to - from) * cw, ch);
            gtk_snapshot_append_color(snapshot, &kSelection, &r);
        }
    }

    if (gtk_widget_has_focus(widget) && self->cursor_line >= top && self->cursor_line < last) {
        graphene_rect_t cursor = GRAPHENE_RECT_INIT(std::min(self->cursor_col, self->cols - 1) * cw,
                                                    (self->cursor_line - top) * ch, cw, ch);
        gtk_snapshot_append_color(snapshot, &kCursor, &cursor);
    }

    for (long l = top; l < last; l++)
        paint_row(self, snapshot, l, static_cast<int>((l - top) * self->cell_height));
}

// --- properties and lifecycle --------------------------------------------------------

void gtklab_grid_set_font(GtklabGrid *self, const char *font) {
    g_return_if_fail(GTKLAB_IS_GRID(self));
    if (!font || self->font == font)
        return;
    self->font = font;
    pango_font_description_free(self->font_desc);
    self->font_desc = pango_font_description_from_string(font);
    g_clear_pointer(&self->attrs, pango_attr_list_unref);
    self->attrs = base_attrs(self);
    measure_cells(self);
    gtk_widget_queue_resize(GTK_WIDGET(self));
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FONT]);
}

const char *gtklab_grid_get_font(GtklabGrid *self) {
    return self->font.c_str();
}

int gtklab_grid_get_columns(GtklabGrid *self) {
    return self->cols;
}

int gtklab_grid_get_rows(GtklabGrid *self) {
    return self->rows;
}

static void gtklab_grid_set_property(GObject *object, guint id, const GValue *value, GParamSpec *pspec) {
    auto *self = GTKLAB_GRID(object);
    switch (id) {
    case PROP_FONT:
        gtklab_grid_set_font(self, g_value_get_string(value));
        break;
    case PROP_HADJUSTMENT:
        set_hadjustment(self, GTK_ADJUSTMENT(g_value_get_object(value)));
        break;
    case PROP_VADJUSTMENT:
        set_vadjustment(self, GTK_ADJUSTMENT(g_value_get_object(value)));
        break;
    case PROP_HSCROLL_POLICY:
        self->hscroll_policy = static_cast<GtkScrollablePolicy>(g_value_get_enum(value));
        break;
    case PROP_VSCROLL_POLICY:
        self->vscroll_policy = static_cast<GtkScrollablePolicy>(g_value_get_enum(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, pspec);
    }
}

static void gtklab_grid_get_property(GObject *object, guint id, GValue *value, GParamSpec *pspec) {
    auto *self = GTKLAB_GRID(object);
    switch (id) {
    case PROP_FONT:
        g_value_set_string(value, self->font.c_str());
        break;
    case PROP_HADJUSTMENT:
        g_value_set_object(value, self->hadjustment);
        break;
    case PROP_VADJUSTMENT:
        g_value_set_object(value, self->vadjustment);
        break;
    case PROP_HSCROLL_POLICY:
        g_value_set_enum(value, self->hscroll_policy);
        break;
    case PROP_VSCROLL_POLICY:
        g_value_set_enum(value, self->vscroll_policy);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, pspec);
    }
}

static void gtklab_grid_dispose(GObject *object) {
    auto *self = GTKLAB_GRID(object);
    if (self->vadjustment) {
        g_signal_handlers_disconnect_by_data(self->vadjustment, self);
        g_clear_object(&self->vadjustment);
    }
    g_clear_object(&self->hadjustment);
    g_clear_pointer(&self->attrs, pango_attr_list_unref);
    G_OBJECT_CLASS(gtklab_grid_parent_class)->dispose(object);
}

static void gtklab_grid_finalize(GObject *object) {
    auto *self = GTKLAB_GRID(object);
    g_clear_pointer(&self->font_desc, pango_font_description_free);
    self->font.~basic_string();
    self->lines.~deque();
    self->pending.~basic_string();
    G_OBJECT_CLASS(gtklab_grid_parent_class)->finalize(object);
}

static void gtklab_grid_class_init(GtklabGridClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = gtklab_grid_set_property;
    object_class->get_property = gtklab_grid_get_property;
    object_class->dispose = gtklab_grid_dispose;
    object_class->finalize = gtklab_grid_finalize;

    widget_class->measure = gtklab_grid_measure;
    widget_class->size_allocate = gtklab_grid_size_allocate;
    widget_class->snapshot = gtklab_grid_snapshot;

    props[PROP_FONT] = g_param_spec_string("font", nullptr, nullptr, "Monospace 11",
                                           static_cast<GParamFlags>(G_PARAM_READWRITE |
                                                                    G_PARAM_EXPLICIT_NOTIFY |
                                                                    G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, N_PROPS, props);

    // GtkScrollable declares these; an implementation overrides them.
    g_object_class_override_property(object_class, PROP_HADJUSTMENT, "hadjustment");
    g_object_class_override_property(object_class, PROP_VADJUSTMENT, "vadjustment");
    g_object_class_override_property(object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
    g_object_class_override_property(object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

    signals[SIGNAL_RESIZED] = g_signal_new("resized", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                                           nullptr, nullptr, nullptr, G_TYPE_NONE, 2, G_TYPE_INT,
                                           G_TYPE_INT);
    signals[SIGNAL_COMMIT] = g_signal_new("commit", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                                          nullptr, nullptr, nullptr, G_TYPE_NONE, 1, G_TYPE_BYTES);

    gtk_widget_class_set_css_name(widget_class, "gtklabgrid");
}

static void gtklab_grid_init(GtklabGrid *self) {
    new (&self->font) std::string();
    new (&self->lines) std::deque<Line>();
    new (&self->pending) std::string();
    self->font_desc = nullptr;
    self->attrs = nullptr;
    self->cols = 0;
    self->rows = 0;
    self->cursor_line = 0;
    self->cursor_col = 0;
    self->hadjustment = nullptr;
    self->vadjustment = nullptr;
    self->hscroll_policy = GTK_SCROLL_MINIMUM;
    self->vscroll_policy = GTK_SCROLL_MINIMUM;
    self->has_selection = FALSE;

    GtkWidget *widget = GTK_WIDGET(self);
    gtk_widget_set_focusable(widget, TRUE);
    gtk_widget_set_hexpand(widget, TRUE);
    gtk_widget_set_vexpand(widget, TRUE);
    gtk_widget_set_overflow(widget, GTK_OVERFLOW_HIDDEN);

    GtkEventController *focus = gtk_event_controller_focus_new();
    g_signal_connect_swapped(focus, "enter", G_CALLBACK(gtk_widget_queue_draw), self);
    g_signal_connect_swapped(focus, "leave", G_CALLBACK(gtk_widget_queue_draw), self);
    gtk_widget_add_controller(widget, focus);

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), self);
    gtk_widget_add_controller(widget, keys);

    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), self);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), self);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(drag));

    gtklab_grid_set_font(self, "Monospace 11");
    resize_screen(self, 80, 24);
}

GtkWidget *gtklab_grid_new(void) {
    return GTK_WIDGET(g_object_new(GTKLAB_TYPE_GRID, nullptr));
}
