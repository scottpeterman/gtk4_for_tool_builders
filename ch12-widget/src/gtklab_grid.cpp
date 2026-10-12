#include "gtklab_grid.h"

#include <algorithm>
#include <new>
#include <string>
#include <vector>

// Colours are fixed until Chapter 15 makes them theme properties.
static const GdkRGBA kBackground = {0.12f, 0.12f, 0.14f, 1.0f};
static const GdkRGBA kForeground = {0.85f, 0.86f, 0.88f, 1.0f};
static const GdkRGBA kCursor = {0.55f, 0.75f, 1.0f, 1.0f};

using Line = std::vector<gunichar>;

struct _GtklabGrid {
    GtkWidget parent_instance;

    // GObject zero-fills the instance and runs no C++ constructors. The C++
    // members are built with placement new in init and destroyed in finalize.
    std::string font;
    std::vector<Line> screen;  // rows x cols, ' ' for empty
    std::string pending;       // incomplete UTF-8 or escape sequence

    PangoFontDescription *font_desc;  // owned
    PangoAttrList *attrs;             // owned
    int cell_width;
    int cell_height;
    int cols;
    int rows;
    int cursor_row;
    int cursor_col;
};

enum { PROP_0, PROP_FONT, N_PROPS };
static GParamSpec *props[N_PROPS];

enum { SIGNAL_RESIZED, N_SIGNALS };
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE(GtklabGrid, gtklab_grid, GTK_TYPE_WIDGET)

// --- cell size -----------------------------------------------------------------

// Base attributes for every layout: the font, and no ligatures. With
// ligatures on, "->" or "==" can become one glyph wider or narrower than two
// cells, and every character after it lands off the grid.
static PangoAttrList *base_attrs(GtklabGrid *self) {
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_font_desc_new(self->font_desc));
    pango_attr_list_insert(attrs, pango_attr_font_features_new("liga 0, calt 0"));
    return attrs;
}

// Cell size = the size of "M" in the widget's font.
static void measure_cells(GtklabGrid *self) {
    PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), "M");
    pango_layout_set_attributes(layout, self->attrs);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(layout, &w, &h);
    g_object_unref(layout);
    self->cell_width = std::max(w, 1);
    self->cell_height = std::max(h, 1);
}

// --- the cell model ------------------------------------------------------------

static void resize_screen(GtklabGrid *self, int cols, int rows) {
    // Keep the bottom of the screen when it shrinks: that is where the
    // cursor and the newest output are.
    const int drop = std::max(0, std::min(static_cast<int>(self->screen.size()) - rows,
                                          self->cursor_row - rows + 1));
    if (drop > 0) {
        self->screen.erase(self->screen.begin(), self->screen.begin() + drop);
        self->cursor_row -= drop;
    }
    self->screen.resize(static_cast<size_t>(rows), Line(static_cast<size_t>(cols), ' '));
    for (Line &line : self->screen)
        line.resize(static_cast<size_t>(cols), ' ');
    self->cols = cols;
    self->rows = rows;
    self->cursor_row = std::clamp(self->cursor_row, 0, rows - 1);
    self->cursor_col = std::clamp(self->cursor_col, 0, cols - 1);
}

// Pixels -> cells. Emits "resized" when the shape changes.
static void update_grid(GtklabGrid *self, int width, int height) {
    const int cols = std::max(width / self->cell_width, 1);
    const int rows = std::max(height / self->cell_height, 1);
    if (cols == self->cols && rows == self->rows)
        return;
    resize_screen(self, cols, rows);
    g_debug("update_grid: %dx%d px -> %d cols x %d rows", width, height, cols, rows);
    g_signal_emit(self, signals[SIGNAL_RESIZED], 0, cols, rows);
}

static void scroll_up(GtklabGrid *self) {
    self->screen.erase(self->screen.begin());
    self->screen.push_back(Line(static_cast<size_t>(self->cols), ' '));
}

static void newline(GtklabGrid *self) {
    if (self->cursor_row + 1 < self->rows)
        self->cursor_row++;
    else
        scroll_up(self);
}

static void put_char(GtklabGrid *self, gunichar c) {
    if (self->cursor_col >= self->cols) {  // wrap
        self->cursor_col = 0;
        newline(self);
    }
    self->screen[static_cast<size_t>(self->cursor_row)][static_cast<size_t>(self->cursor_col)] = c;
    self->cursor_col++;
}

void gtklab_grid_put_text(GtklabGrid *self, int row, int col, const char *utf8) {
    g_return_if_fail(GTKLAB_IS_GRID(self));
    if (row < 0 || row >= self->rows)
        return;
    for (const char *p = utf8; *p && col < self->cols; p = g_utf8_next_char(p), col++)
        if (col >= 0)
            self->screen[static_cast<size_t>(row)][static_cast<size_t>(col)] = g_utf8_get_char(p);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

// Length of a complete escape sequence at the start of s, 0 if incomplete.
// Recognises CSI (ESC [ ... final) and OSC (ESC ] ... BEL or ESC \); any
// other ESC x is two bytes.
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
    std::string &buf = self->pending;
    buf.append(data, len);

    size_t i = 0;
    while (i < buf.size()) {
        const unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c == 0x1b) {
            const size_t n = escape_length(buf, i);
            if (n == 0)
                break;  // wait for the rest
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
            i++;  // other control characters: ignore
        } else {
            const gunichar u = g_utf8_get_char_validated(buf.data() + i, static_cast<gssize>(buf.size() - i));
            if (u == static_cast<gunichar>(-2))
                break;  // incomplete sequence: wait for more bytes
            if (u == static_cast<gunichar>(-1)) {
                put_char(self, 0xfffd);  // invalid byte
                i++;
                continue;
            }
            put_char(self, u);
            i = static_cast<size_t>(g_utf8_next_char(buf.data() + i) - buf.data());
        }
    }
    buf.erase(0, i);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

// --- GtkWidget virtual functions ---------------------------------------------

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

static void paint_row(GtklabGrid *self, GtkSnapshot *snapshot, int row) {
    std::string text;
    for (gunichar c : self->screen[static_cast<size_t>(row)]) {
        char utf8[6];
        text.append(utf8, static_cast<size_t>(g_unichar_to_utf8(c, utf8)));
    }
    PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), text.c_str());
    pango_layout_set_attributes(layout, self->attrs);

    gtk_snapshot_save(snapshot);
    graphene_point_t at = GRAPHENE_POINT_INIT(0.f, static_cast<float>(row * self->cell_height));
    gtk_snapshot_translate(snapshot, &at);
    gtk_snapshot_append_layout(snapshot, layout, &kForeground);
    gtk_snapshot_restore(snapshot);
    g_object_unref(layout);
}

// Records drawing operations; GTK renders them later. Never called directly:
// gtk_widget_queue_draw asks the frame clock to call it on the next frame.
static void gtklab_grid_snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
    auto *self = GTKLAB_GRID(widget);
    const int w = gtk_widget_get_width(widget);
    const int h = gtk_widget_get_height(widget);

    graphene_rect_t all = GRAPHENE_RECT_INIT(0.f, 0.f, static_cast<float>(w), static_cast<float>(h));
    gtk_snapshot_append_color(snapshot, &kBackground, &all);

    if (gtk_widget_has_focus(widget)) {
        graphene_rect_t cursor = GRAPHENE_RECT_INIT(
            static_cast<float>(std::min(self->cursor_col, self->cols - 1) * self->cell_width),
            static_cast<float>(self->cursor_row * self->cell_height),
            static_cast<float>(self->cell_width), static_cast<float>(self->cell_height));
        gtk_snapshot_append_color(snapshot, &kCursor, &cursor);
    }

    for (int row = 0; row < self->rows; row++)
        paint_row(self, snapshot, row);
}

// --- properties and lifecycle --------------------------------------------------

void gtklab_grid_set_font(GtklabGrid *self, const char *font) {
    g_return_if_fail(GTKLAB_IS_GRID(self));
    if (!font || self->font == font)
        return;
    self->font = font;
    pango_font_description_free(self->font_desc);
    self->font_desc = pango_font_description_from_string(font);
    pango_attr_list_unref(self->attrs);
    self->attrs = base_attrs(self);
    measure_cells(self);
    // New cell size: re-measure, re-allocate (-> update_grid), redraw.
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
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, pspec);
    }
}

// dispose: drop references to other objects (may run more than once).
static void gtklab_grid_dispose(GObject *object) {
    auto *self = GTKLAB_GRID(object);
    g_clear_pointer(&self->attrs, pango_attr_list_unref);
    G_OBJECT_CLASS(gtklab_grid_parent_class)->dispose(object);
}

// finalize: free everything else (runs once, last).
static void gtklab_grid_finalize(GObject *object) {
    auto *self = GTKLAB_GRID(object);
    g_clear_pointer(&self->font_desc, pango_font_description_free);
    self->font.~basic_string();
    self->screen.~vector();
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

    signals[SIGNAL_RESIZED] = g_signal_new("resized", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                                           nullptr, nullptr, nullptr, G_TYPE_NONE, 2, G_TYPE_INT,
                                           G_TYPE_INT);

    gtk_widget_class_set_css_name(widget_class, "gtklabgrid");
}

static void gtklab_grid_init(GtklabGrid *self) {
    new (&self->font) std::string();
    new (&self->screen) std::vector<Line>();
    new (&self->pending) std::string();
    self->font_desc = nullptr;
    self->attrs = nullptr;
    self->cols = 0;
    self->rows = 0;
    self->cursor_row = 0;
    self->cursor_col = 0;

    gtk_widget_set_focusable(GTK_WIDGET(self), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(self), TRUE);
    gtk_widget_set_overflow(GTK_WIDGET(self), GTK_OVERFLOW_HIDDEN);

    // Focus changes the cursor, so redraw on focus in and out.
    GtkEventController *focus = gtk_event_controller_focus_new();
    g_signal_connect_swapped(focus, "enter", G_CALLBACK(gtk_widget_queue_draw), self);
    g_signal_connect_swapped(focus, "leave", G_CALLBACK(gtk_widget_queue_draw), self);
    gtk_widget_add_controller(GTK_WIDGET(self), focus);

    gtklab_grid_set_font(self, "Monospace 11");
    resize_screen(self, 80, 24);
}

GtkWidget *gtklab_grid_new(void) {
    return GTK_WIDGET(g_object_new(GTKLAB_TYPE_GRID, nullptr));
}
