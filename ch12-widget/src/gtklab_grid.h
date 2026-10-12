// GtklabGrid: a character-cell widget. It shows text in a fixed-pitch grid
// and reports its size in cells. It is not a terminal emulator: it handles
// printable text, \r, \n, \b and \t, and skips escape sequences.
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTKLAB_TYPE_GRID (gtklab_grid_get_type())
G_DECLARE_FINAL_TYPE(GtklabGrid, gtklab_grid, GTKLAB, GRID, GtkWidget)

GtkWidget *gtklab_grid_new(void);

// The "font" property: a Pango description string such as "Monospace 11".
void gtklab_grid_set_font(GtklabGrid *grid, const char *font);
const char *gtklab_grid_get_font(GtklabGrid *grid);

// Write UTF-8 text into the cell model at (row, col), clipped to the grid.
void gtklab_grid_put_text(GtklabGrid *grid, int row, int col, const char *utf8);

// Feed bytes from a pty: printable text at the cursor, with wrapping and
// scrolling. Partial UTF-8 sequences are kept until the rest arrives.
void gtklab_grid_feed(GtklabGrid *grid, const char *data, gsize len);

int gtklab_grid_get_columns(GtklabGrid *grid);
int gtklab_grid_get_rows(GtklabGrid *grid);

// Signals:
//   "resized" (GtklabGrid *, int columns, int rows): the grid changed shape.

G_END_DECLS
