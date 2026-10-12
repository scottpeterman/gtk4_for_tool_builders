# Chapter 13 — Scrollback, keys and selection in your widget

The grid grows the three things that make a terminal usable: a scrollback
you can wheel through, keys typed straight into it instead of into an input
line, and text you can select, copy and paste. Each is a GTK mechanism you
have used from the outside and now implement from the inside: a scrollable
widget, a key controller, and a drag gesture with the clipboard.

::: {.box title="Recall"}
Which phase do the page's right-click and zoom controllers run in, and why
(Chapter 9)? When does `snapshot` run (Chapter 12)? At what priority does the
pty watch run, and why (Chapter 11)?
:::

## The idea: a scrollable widget

A `GtkScrolledWindow` can scroll any child, but it can scroll a child in two
ways. A child that is simply bigger than the viewport is drawn whole and
moved; that is wasteful for a terminal with thousands of lines of history.
A child that implements the **`GtkScrollable`** interface is given two
`GtkAdjustment` objects instead, one per direction, and draws only what the
adjustment says is visible. An adjustment is a value with a range: `lower`,
`upper`, `page_size` and the current `value`. The scrolled window draws its
scrollbar from them and moves `value` when the user scrolls; the widget keeps
the range current and redraws when `value` changes.

The units are the widget's choice. GtklabGrid uses lines: `upper` is the
number of lines held, scrollback and screen together; `page_size` is the
number of rows on screen; `value` is the first line shown.

Implementing an interface is declared in the type definition, with
`G_DEFINE_FINAL_TYPE_WITH_CODE` and `G_IMPLEMENT_INTERFACE`.
`GtkScrollable` declares four properties (`hadjustment`, `vadjustment`,
`hscroll-policy`, `vscroll-policy`); an implementation overrides them in
`class_init` and stores what it is given.

## The idea: keys become bytes

Keys reach a focused widget through a `GtkEventControllerKey`. The grid turns
each key into the bytes a terminal sends for it, Return into `\r`, the up
arrow into `ESC [ A`, Ctrl+C into byte 3, and hands them out through a
`commit` signal. The grid never writes to the pty itself; the tab connects
`commit` to the session. Keeping the widget ignorant of where its bytes go is
what lets Chapter 14 swap it out.

The window's capture-phase shortcut controller from Chapter 7 runs before
the grid's key controller, so gtklab's own shortcuts never become bytes.

## Build

The interface grows selection, the clipboard and the `commit` signal:

```{.cpp include="ch13-input/src/gtklab_grid.h" from="// Selection and clipboard." to="//             (keys and pastes). The grid never writes anywhere itself."}
```

### Scrollback and the adjustments

The cell model becomes a `std::deque<Line>` of scrollback followed by the
screen, capped at 5,000 lines of history. The type definition declares the
interface:

```{.cpp include="ch13-input/src/gtklab_grid.cpp" from="G_DEFINE_FINAL_TYPE_WITH_CODE" to="G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, nullptr))"}
```

and `class_init` overrides the interface's properties:

```{.cpp include="ch13-input/src/gtklab_grid.cpp" from="// GtkScrollable declares these" to="g_object_class_override_property(object_class, PROP_VSCROLL_POLICY, \"vscroll-policy\");"}
```

Keeping the range current is one function, called whenever lines are added
or the size changes. Its `follow` flag decides whether the view stays at the
bottom: new output keeps the view at the newest line only if the user was
already there, so someone scrolled back to read is not yanked down:

```{.cpp include="ch13-input/src/gtklab_grid.cpp" fn="static void configure_adjustment("}
```

When the scrolled window hands the grid an adjustment, the grid takes a
reference, listens for changes, and configures it:

```{.cpp include="ch13-input/src/gtklab_grid.cpp" fn="static void set_vadjustment("}
```

`snapshot` now draws from `top_line`, the adjustment's value, instead of the
first screen row.

### Keys

```{.cpp include="ch13-input/src/gtklab_grid.cpp" fn="static gboolean on_key_pressed("}
```

### Selection and the clipboard

A `GtkGestureDrag` records the cell under the press and the cell under the
pointer as it moves, in absolute line numbers so that a selection survives
scrolling. Copying joins the selected lines and trims trailing blanks, which
are padding, not content:

```{.cpp include="ch13-input/src/gtklab_grid.cpp" fn="char *gtklab_grid_get_selected_text("}
```

Pasting is asynchronous: the clipboard may belong to another program, and
reading it takes a round trip. The grid could be destroyed before the text
arrives, so the request holds a reference to it until the callback runs:

```{.cpp include="ch13-input/src/gtklab_grid.cpp" fn="static void on_paste_ready("}
```

```{.cpp include="ch13-input/src/gtklab_grid.cpp" fn="void gtklab_grid_paste_clipboard("}
```

### In the app

The grid goes inside a `GtkScrolledWindow`, which becomes the notebook page;
the input line is deleted; and the grid's `commit` goes to the session:

```{.cpp include="ch13-input/src/app_window.cpp" from="// Keys and pastes from the grid go to the child." to="tab);"}
```

Copy, Paste and Select All (Ctrl+Shift+C, V and A) become actions that call
the grid.

One more focus fix, found by the same scripted testing as Chapter 11's. When
a popover menu closes, focus goes to the notebook's tab strip, so the next
keys typed go nowhere. `on_popover_closed` now hands focus back to the
current tab's view, on idle:

```{.cpp include="ch13-input/src/app_window.cpp" fn="void AppWindow::on_popover_closed("}
```

## Done when

The wheel scrolls back through 300 lines of `seq 1 300`, a drag selects
across lines, and Ctrl+Shift+C followed by Ctrl+Shift+V pastes the selection
at the prompt.

![Left: scrolled back, with lines 166 to 171 selected. Right: after Ctrl+Shift+C, typing `echo pasted: ` and Ctrl+Shift+V.](images/ch13-select.png)

Look closely at what the paste did. Each pasted line ran as a command,
because the paste turned each line break into Enter. That is how a paste
into a terminal behaves unless both sides agree otherwise, and it is why
gtkpyte supports **bracketed paste**: when the program in the terminal asks
for it, the paste is wrapped in markers so the shell inserts it as text
instead of running it.

::: {.box title="Debugger drill"}
Set a breakpoint in `on_key_pressed`, focus a grid, and press Ctrl+Shift+Q.
gtklab quits and the breakpoint is never hit: the window's capture-phase
shortcut controller handled the key before the grid saw it. The scratch
config's log shows the same thing: no `grid key` line after the keypress.
:::

::: {.box title="Break it"}
Remove the call to `configure_adjustment` at the end of `gtklab_grid_feed`.
Run `seq 1 300` and scroll down as far as you like. The view stays on lines 1
to 27. The scrolled window still believes there is one screen of lines,
because nothing told it otherwise.
:::

![The Break it: 300 lines printed, and the view stuck at the top.](images/ch13-breakit.png){width=70%}

## Common mistakes

**Scrolling by pixels in a terminal.** Lines are the natural unit, and they
make the selection and the redraw arithmetic simple.

**Always following the output.** A user who scrolled back to read something
is pulled to the bottom by every new line. Follow only when already at the
bottom.

**Writing to the pty from the widget.** The widget then knows about ptys,
and cannot be reused for ssh channels, recorded sessions or tests. Emit a
signal.

**A paste callback that assumes its widget is still alive.** It may not be.
Hold a reference for the round trip.

::: {.box title="Read in tmssh"}
In gtkpyte: `on_click_pressed` (selection, including double- and
triple-click), `on_scroll`, `on_key_pressed`, and the adjustment properties
in `gtkpyte_terminal_class_init`.
:::

::: {.box title="Check yourself"}
What do an adjustment's `upper`, `page_size` and `value` mean for the grid?
Why does the grid emit `commit` instead of writing to the pty? Why does the
paste request hold a reference?
:::
