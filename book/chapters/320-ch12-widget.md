# Chapter 12 — Writing a widget

The text view standing in for a terminal goes, and in its place comes a
widget you write yourself: `GtklabGrid`, a fixed-pitch character grid that
knows its size in rows and columns and tells the pty when it changes. It is
not a terminal emulator; it handles printable text, carriage return, line
feed, backspace and tab, and skips escape sequences. That is enough to learn
how a GTK 4 widget is measured, sized and drawn, which is the point.

::: {.box title="Recall"}
What does a factory's `bind` do (Chapter 5)? Name the macro pair that
declares and defines a GObject type (Chapter 5). Which side of the split owns
the model (Chapter 4)?
:::

## The idea: measure, allocate, snapshot

A custom widget is a GObject subclass of `GtkWidget` that overrides a few
virtual functions in its class structure. Three of them carry the work:

- **`measure`** answers "how big would you like to be?", once for each
  orientation, with a minimum and a natural size.
- **`size_allocate`** says "this is how big you are", with a width and a
  height. A widget that has children positions them here.
- **`snapshot`** records what the widget looks like, as a list of drawing
  operations: fill this rectangle with this colour, draw this text here.

You never call these yourself, and you never draw on demand. When something
changes the widget's size, you call `gtk_widget_queue_resize`; when
something changes only its appearance, `gtk_widget_queue_draw`. GTK's
**frame clock** then runs, at most once per frame, a layout pass that
measures and allocates, and a paint pass that calls `snapshot` and renders
the recorded operations on the GPU.

![The two pipelines. Changing the font changes the size; new output changes only the appearance. Both end at the frame clock.](diagrams/widget-pipeline.svg)

Drawing text means Pango, GTK's text layout library. A `PangoLayout` holds a
string, a font and attributes, and knows how large it renders;
`gtk_widget_create_pango_layout` makes one with the widget's font context,
and `gtk_snapshot_append_layout` draws it.

A widget can also have **properties** (named, typed values that can be set
and watched, here `font`) and **signals** of its own (here `resized`). Both
are declared in `class_init`, once per type.

## Build

The interface:

```{.cpp include="ch12-widget/src/gtklab_grid.h"}
```

### The instance and the class

The instance struct starts with the parent instance, as every GObject does,
and holds C++ members that are built with placement `new` in `init` and
destroyed in `finalize`, exactly as Chapter 5's item did:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" from="struct _GtklabGrid {" to="};"}
```

`class_init` installs the virtual functions, the `font` property and the
`resized` signal, and gives the widget a CSS name so that themes can style
it:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void gtklab_grid_class_init("}
```

`init` builds the C++ members, makes the widget focusable and expanding,
redraws when focus comes and goes (the cursor depends on it), and sets the
initial font and grid:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void gtklab_grid_init("}
```

`dispose` and `finalize` split cleanup in two. `dispose` drops references to
other objects and may run more than once; `finalize` frees everything else
and runs exactly once, last:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void gtklab_grid_dispose("}
```

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void gtklab_grid_finalize("}
```

### The cell size

A character grid needs a fixed cell. It is the size of "M" in the widget's
font, measured with the same attributes every row will be drawn with. Those
attributes turn ligatures off: with them on, programming fonts draw `->` or
`==` as a single glyph whose width is not two cells, and every character
after it lands off the grid.

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static PangoAttrList *base_attrs("}
```

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void measure_cells("}
```

Setting the font re-measures the cell and asks for a new layout pass, which
will allocate the widget again and so recompute the grid:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="void gtklab_grid_set_font("}
```

### Size

`measure` asks for twenty columns and five rows as a minimum, and eighty by
twenty-four as natural:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void gtklab_grid_measure("}
```

`size_allocate` converts pixels to cells and emits `resized` only when the
shape actually changes:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void update_grid("}
```

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void gtklab_grid_size_allocate("}
```

### Drawing

`snapshot` fills the background, draws the cursor when the widget has focus,
and draws each row as one Pango layout, translated to its row:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void paint_row("}
```

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="static void gtklab_grid_snapshot("}
```

### Bytes in

`gtklab_grid_feed` takes bytes from the pty. It keeps an incomplete UTF-8
character or escape sequence until the rest arrives, because a read can end
anywhere; it skips CSI and OSC escape sequences; and it ends by queueing a
draw:

```{.cpp include="ch12-widget/src/gtklab_grid.cpp" fn="void gtklab_grid_feed("}
```

### In the app

The grid replaces the text view. The pty's output goes to
`gtklab_grid_feed`, and the grid's `resized` signal goes to the session, so
the child learns its new size:

```{.cpp include="ch12-widget/src/app_window.cpp" from="GtklabGrid *grid = GTKLAB_GRID(tab->view);" to="tab);"}
```

The session starts at the grid's current size rather than a fixed 80 by 24.
The input line under the grid stays for one more chapter.

## Done when

Output appears in the grid in a monospace font, and `stty size` in the shell
follows the window as it is resized.

![GtklabGrid. `stty size` reported 26 rows by 70 columns, then 32 by 93 after the window was enlarged.](images/ch12-grid.png){width=80%}

::: {.box title="Debugger drill"}
Set a breakpoint in `update_grid` and resize the window. In the recorded run
the first allocation was `637x480 px -> 70 cols x 26 rows` and the resized
one `837x580 px -> 93 cols x 32 rows`, each followed by the `resized`
emission and, in the shell, a new `stty size`. Then set a breakpoint in
`gtklab_grid_snapshot` with a hit count of 1, type a command and read the
stack when it stops. It starts at the frame clock, not at your feed
function: `queue_draw` only asked.
:::

::: {.box title="Break it"}
Delete the `gtk_widget_queue_draw` at the end of `gtklab_grid_feed`. Run
`echo one; echo two`. The grid stays blank, prompt included. The text is all
in the cell model; nothing asked for a frame. Resize the window, which does
ask for one, and everything appears at once.
:::

![The Break it. Top: after the echo, nothing. Bottom: the same grid after a resize.](images/ch12-breakit.png){width=70%}

## Common mistakes

**Drawing in a signal handler.** There is nothing to draw on outside
`snapshot`. Change state, queue a draw, and let `snapshot` read the state.

**`queue_draw` when the size changed.** A new font needs a new layout pass:
`queue_resize`, which also redraws.

**Chaining up in the wrong place.** `dispose` and `finalize` must call the
parent class's version, at the end.

**Ligatures in a grid.** Turn them off for anything that must line up in
columns.

::: {.box title="Read in tmssh"}
gtkpyte's terminal in `gtkpyte/src/gtkpyte-terminal.cpp`:
`gtkpyte_terminal_class_init`, `gtkpyte_terminal_size_allocate`,
`update_grid`, `gtkpyte_terminal_snapshot`, `paint_row`, `measure_cells` and
`base_attrs`. The names match on purpose; gtkpyte also draws colours, bold,
underline, wide characters and a selection.
:::

::: {.box title="Check yourself"}
What do `measure`, `size_allocate` and `snapshot` each answer? When does
`snapshot` run? Why does setting the font call `queue_resize` rather than
`queue_draw`?
:::
