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

::: {.box title="In this chapter"}
- Subclass `GtkWidget` and override `measure`, `size_allocate` and
  `snapshot`, knowing what question each one answers.
- Report a minimum and a natural size, and turn whatever size you are given
  into rows and columns.
- Change a widget's state and ask for a frame, instead of drawing.
- Measure and draw monospace text with Pango so that it stays on a grid.
- Give a widget a property and a signal, so that the code around it never
  needs to know its insides.
:::

## Concepts

### A widget answers three questions

A custom widget is a GObject subclass of `GtkWidget` that overrides a few
virtual functions in its class structure. Three of them carry the work:

| Virtual function | The question | GtklabGrid's answer | Called |
| --- | --- | --- | --- |
| `measure` | How big would you like to be, in this orientation? | 20 × 5 cells at least, 80 × 24 by preference | when a layout pass needs it |
| `size_allocate` | This is how big you are. | divide the pixels by the cell size | after the parent has decided |
| `snapshot` | What do you look like? | a background, a cursor, one text layout per row | when a frame needs a new recording |

You never call these yourself. GTK calls them, in that order, when it lays
out and paints the window. A widget with children also positions them in
`size_allocate`; GtklabGrid has none.

The rest of the widget's life brackets these calls. `init` runs once, when
the instance is created; `dispose` and `finalize` run at the end. Between
them, the widget is **realized** when it is put on a display, and only then
does its Pango context carry that display's font settings. gtkpyte measures
its cell again in a `realize` handler for that reason: metrics taken in
`init` can be off by a pixel.

### Size is negotiated, top down

A widget does not choose its size; it states a range and the parent chooses.
`measure` is called once per orientation and fills in two numbers: the
**minimum**, below which the widget cannot work, and the **natural** size,
which it would take if space were free. The parent adds up its children's
answers, the window does the same for its child, and the window's own size
decides how much there is to share. Then allocation runs back down: each
parent calls `size_allocate` on its children with the space it gave them.

![Size negotiation. The parent asks, the grid answers in pixels derived from cells, the parent decides, and the grid turns the pixels back into cells and tells the pty.](diagrams/ch12-negotiation.svg)

The consequence for a grid is that the allocation is almost never the
natural size. It is larger when the window has room to spare, since
GtklabGrid sets `hexpand` and `vexpand`, and smaller when the sidebar and the
window edges take their share. So `size_allocate` must accept any size and
make the best of it: integer division gives the whole cells, and the
remainder is a margin at the right and bottom that the background fills. In
the recorded run, 637 by 480 pixels became 70 columns by 26 rows of
9-by-18-pixel cells, ten columns short of the natural 80, with 7 pixels left
over at the right and 12 at the bottom.

`measure` also receives a `for_size` argument, the size already fixed in the
other orientation, and a widget can declare through `get_request_mode` that
its height depends on its width. A wrapping label works that way: the
narrower it is, the taller it gets. A grid has no such trade, so GtklabGrid
ignores `for_size` and keeps the default request mode, which a test program
reported as `GTK_SIZE_REQUEST_CONSTANT_SIZE`.

### Queue, don't draw

When something changes the widget's size, you call
`gtk_widget_queue_resize`; when something changes only its appearance,
`gtk_widget_queue_draw`. GTK's **frame clock** then runs, at most once per
frame, a layout pass that measures and allocates, and a paint pass that calls
`snapshot` and renders the recorded operations. The frame runs from the main
loop at `GDK_PRIORITY_REDRAW`, which is why Chapter 11's pty watch sits below
it.

![The two pipelines. Changing the font changes the size; new output changes only the appearance. Both end at the frame clock.](diagrams/widget-pipeline.svg)

`snapshot` does not draw pixels. It records a list of operations, a tree of
render nodes, and GTK keeps the recording. A widget that has not asked for a
new frame is not asked to record again: in a test program, a widget beside a
label whose text changed twenty times was snapshotted once, and one
`gtk_widget_queue_draw` on it produced exactly one more snapshot. That is why
you never draw from a signal handler and why forgetting `queue_draw` leaves
stale content on screen: the old recording is still valid as far as GTK
knows.

| What changed | What you call | What runs on the next frame |
| --- | --- | --- |
| The font, so the cell size | `gtk_widget_queue_resize` | `measure`, `size_allocate`, `snapshot` |
| New bytes in the cell model | `gtk_widget_queue_draw` | `snapshot` |
| Focus came or went (the cursor) | `gtk_widget_queue_draw` | `snapshot` |
| The window was resized | nothing: GTK queues it | `size_allocate`, `snapshot` |

Several calls before the frame cost the same as one. `gtklab_grid_feed`
queues a draw for every chunk the pty delivers, and the grid is still
snapshotted at most once per frame.

### Text on a grid

Drawing text means Pango, GTK's text layout library. A `PangoLayout` holds a
string, a font and attributes, and knows how large it renders;
`gtk_widget_create_pango_layout` makes one with the widget's font context,
and `gtk_snapshot_append_layout` draws it.

A terminal grid needs every character in a column to start at the same
pixel. The grid gets that by measuring one cell, the size of "M", and
trusting that a monospace font's other glyphs are as wide. Anything that
changes widths breaks the trust: ligatures, which the grid turns off, and
characters that a monospace font draws two cells wide, which GtklabGrid
does not handle and gtkpyte does.

### Properties and signals: the widget's outward face

A widget can also have **properties** (named, typed values that can be set
and watched, here `font`) and **signals** of its own (here `resized`). Both
are declared in `class_init`, once per type.

The choice of what to put in them is a design decision. `resized` carries
columns and rows, not pixels, because cells are what the pty needs and
pixels are the widget's private business. And the grid emits a signal
rather than calling the session, so it knows nothing about ptys; the tab
connects the two. Chapter 13 sends keys out the same way, and Chapter 14
replaces the widget without touching the session.

### Where the C++ lives

GObject allocates an instance with zeroed memory and runs no C++
constructors, so a `std::string` member is not a valid string until
something builds it. There are two ways to deal with that:

| | C++ members in the instance struct | One pointer to a C++ struct |
| --- | --- | --- |
| Used by | GtklabGrid (and Chapter 5's item) | gtkpyte (`State *st`) |
| In `init` | placement `new` for each member | `new State()` |
| In `finalize` | an explicit destructor call for each member | `delete st` |
| Risk | forgetting one member in either list | one extra indirection |

GtklabGrid uses the first so you see exactly what GObject does not do for
you. gtkpyte uses the second because its state has many members with
constructors; one `new` and one `delete` keep the problem in one place.

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

## Review

1. The grid's natural width is 80 cells, and it is allocated 637 pixels with
   9-pixel cells. What does `size_allocate` do with that, and what happens to
   the 7 pixels left over?
2. Setting the font calls `gtk_widget_queue_resize`; feeding bytes calls
   `gtk_widget_queue_draw`. What would go wrong if the font setter called
   only `queue_draw`?
3. In the Break it, the text is in the cell model but the grid stays blank.
   Why does GTK keep showing the old picture rather than asking the grid
   again?
4. Why does `resized` carry columns and rows rather than pixels, and why does
   `update_grid` emit it only when the shape changes?
5. `measure` receives a `for_size` that GtklabGrid ignores. What kind of
   widget would need it, and why does a character grid not?
6. Why must `measure_cells` lay out "M" with the same attribute list that
   `paint_row` uses?
7. The snapshot's stack in the debugger drill starts at the frame clock, not
   at `gtklab_grid_feed`. What does that tell you about when bytes read in
   `on_readable` reach the screen?
8. GtklabGrid builds its C++ members with placement `new` in `init`. What
   would happen to a `std::string` member if `init` did not?

**Questions for deeper thought.** gtkpyte measures its cell again when it is
realized, because the font settings belong to the display. What else in a
widget of yours depends on the display it lands on, such as the scale factor
of a HiDPI screen, and where would you recompute it? If you wrote a widget
for your own tools, say a grid of device states or a latency strip chart,
which of its state would you expose as properties and which events as
signals? `paint_row` builds a fresh Pango layout for every row on every
snapshot: at what size of grid would you start caching, and what would the
cache be keyed on?

## Exercises

1. **A hollow cursor.** In `gtklab_grid_snapshot`
   (`ch12-widget/src/gtklab_grid.cpp`), draw the cursor as an outline with
   `gtk_snapshot_append_border` when the grid does not have focus, instead of
   not drawing it. Done when clicking the sidebar leaves a hollow box at the
   cursor and clicking the grid fills it again.
2. **Columns and rows as properties.** Add read-only `columns` and `rows`
   properties (`G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY`) and notify them
   from `update_grid`. In `ch12-widget/src/app_window.cpp`, connect
   `notify::columns` to a handler that logs the new value. Done when
   resizing the window logs a line only when the column count changes.
3. **Centre the grid.** Store the leftover pixels in `update_grid` and
   translate the whole drawing in `gtklab_grid_snapshot` by half of them in
   each direction. Done when the margins on the left and right differ by at
   most one pixel, checked with the GTK Inspector or a screenshot.
4. **Count the frames.** Add counters to `gtklab_grid_feed` and
   `gtklab_grid_snapshot` and log both every second from a
   `g_timeout_add_seconds` started in `gtklab_grid_init` (and removed in
   `gtklab_grid_dispose`). Run `seq 1000000` in a tab. Done when both counts
   are logged and you can explain, from the frame clock, why they differ.

