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

::: {.box title="In this chapter"}
- Implement `GtkScrollable` and keep a pair of adjustments in step with a
  model whose unit you choose.
- Decide when new output should move the view and when it must leave the
  user where they are.
- Turn key presses into the bytes a terminal sends, and say which bytes the
  kernel changes on the way to the shell.
- Select text with a drag gesture, and copy and paste through the
  asynchronous clipboard without outliving the widget.
:::

## Concepts

### Two ways to scroll

A `GtkScrolledWindow` can scroll any child, but it scrolls a child in one of
two ways. A child that does not implement **`GtkScrollable`** is wrapped in a
`GtkViewport` (a test program that put a label in a scrolled window got a
`GtkViewport` back from `gtk_scrolled_window_get_child`): the child is
allocated its full size, drawn whole, and moved. That is wasteful for a
terminal with thousands of lines of history. A child that implements
`GtkScrollable` is given the scrolled window's two `GtkAdjustment` objects
instead, one per direction, is allocated the visible area (or its minimum
size, if that is larger), and draws only what the adjustment says is
visible. A text view works this way, and
from this chapter so does the grid.

### The adjustment is a contract

An adjustment is a value with a range: `lower`, `upper`, `page_size` and the
current `value`, plus the step and page increments for moving by a line or
a page. It belongs to neither side. The scrolled window draws
its scrollbar from it and moves `value` when the user drags the bar or turns
the wheel; the widget keeps the range current and redraws when `value`
changes.

![The GtkScrollable contract. The scrolled window hands the grid an adjustment; the grid sets the range; either side can move the value, and the grid redraws when it moves.](diagrams/ch13-scrollable.svg)

The units are the widget's choice, and the choice shapes everything after
it. GtklabGrid uses lines:

| Field | In GtklabGrid | Who changes it |
| --- | --- | --- |
| `lower` | 0, the oldest line kept | the grid |
| `upper` | lines held, scrollback and screen together | the grid, as lines arrive or are trimmed |
| `page_size` | rows on screen | the grid, on resize |
| `value` | the first line shown | the scrolled window (wheel, scrollbar), the grid (follow, typing) |

Pixels would work too, and a text view uses them, because its lines are of
different heights. A terminal's lines are all one cell high, and in lines the
arithmetic stays whole: the first line drawn is `value`, a click at pixel
`y` is on line `value + y / cell_height`, and a selection can be stored in
absolute line numbers that do not change when the view scrolls.

The contract has one rule that is easy to miss: the scrolled window cannot
know that the content grew. Every change to the number of lines must go back
into the adjustment, or the scrollbar describes a document that no longer
exists; this chapter's Break it shows exactly that. And because both sides
can move `value`, the widget must decide when moving it is its business.
New output follows the bottom only if the user was already there; a key
press always returns to the bottom, as in every terminal.

Implementing an interface is declared in the type definition, with
`G_DEFINE_FINAL_TYPE_WITH_CODE` and `G_IMPLEMENT_INTERFACE`.
`GtkScrollable` declares four properties (`hadjustment`, `vadjustment`,
`hscroll-policy`, `vscroll-policy`); an implementation overrides them in
`class_init` and stores what it is given. The scrolled window sets them
when the grid becomes its child.

### Keys become bytes

A key press reaches a widget as a **keyval**, GDK's name for the symbol the
key produces with the current layout and Shift already applied (`a`, `A`,
`Return`, `Up`), together with the state of the modifiers. A terminal sends
neither: it sends bytes, the ones a hardware terminal would have sent for
that key. Printable keys become their UTF-8 text; Return becomes a carriage
return; Ctrl with a letter becomes a control character, `a` as 1 through `z`
as 26; keys with no character, like the arrows, become escape sequences.

The grid gets keys from a `GtkEventControllerKey` and hands the bytes out
through its `commit` signal. The grid never writes to the pty itself; the
tab connects `commit` to the session. Keeping the widget ignorant of where
its bytes go is what lets Chapter 14 swap it out, and what would let the
same widget sit on an ssh channel or a test harness.

![A key on its way to the shell. The window's shortcuts see it first; only a key that is not a shortcut becomes bytes, and the kernel changes some of those bytes again.](diagrams/ch13-keys.svg)

Chapter 11's line discipline does the rest, and it explains choices that
look arbitrary from inside the widget. Return sends `\r`, because that is what a
terminal's Return key sends. A program reading lines, such as `cat`, gets
`\n`, because the line discipline turns carriage return into newline on
input; bash at its prompt switches that off and reads the `\r` itself.
Backspace sends byte 127, because that is the pty's erase character. Ctrl+C
sends byte 3, and the kernel, not the grid, turns it into `SIGINT`. And the
grid never draws what you type: the echo comes back through the pty as
output, from the line discipline or the shell. A paste follows the same
path, which is why the grid replaces each newline in pasted text with `\r`,
the byte Return sends.

The mapping is where a toy and a real terminal part ways:

| Key | GtklabGrid sends | gtkpyte sends |
| --- | --- | --- |
| Return | `\r` | `\r` |
| Backspace | byte 127 | byte 127; Ctrl+Backspace sends byte 8 |
| Ctrl+C | byte 3 | byte 3 |
| Up | `ESC [ A` | `ESC [ A`, or `ESC O A` when the application asked for cursor-key mode; `ESC [ 1 ; 5 A` with Ctrl |
| Alt+b | `b` (Alt is ignored) | `ESC b` |
| Page Up, F5, Shift+Tab | nothing: the key is not handled | `ESC [ 5 ~`, `ESC [ 15 ~`, `ESC [ Z` |

The last column depends on state the application set by sending escape
sequences, which GtklabGrid skips and gtkpyte tracks. That is Chapter 14's
subject.

Neither widget creates an input method context (`GtkIMContext`), which is
how GTK delivers text composed from several keys, such as accented letters
from a compose key or Chinese and Japanese input. Both see key presses only.
For a tool aimed at network engineers typing ASCII commands, that is a
reasonable cut; for a general-purpose terminal it is a gap.

### Who sees a key first

Chapter 9's propagation decides which keys reach the grid at all. The
window's shortcut controller from Chapter 7 runs in the capture phase, before
the grid's key controller, so gtklab's own shortcuts never become bytes.
That gives a terminal a policy question every terminal answers the same way:
the plain Ctrl combinations belong to the program in the terminal (Ctrl+C
interrupts, Ctrl+V is a literal-next in the shell), so the application's own
commands live on Ctrl+Shift. Copy, Paste and Select All here are
Ctrl+Shift+C, V and A.

### Selection and the clipboard

A selection is model state like any other: two cell positions, where the
drag started and where the pointer is now. `snapshot` reads it to paint a
translucent band under the text, and copying reads the same cells back out
as text. A `GtkGestureDrag` supplies the positions: a start point on
`drag-begin` and an offset from it on each `drag-update`. Nothing about the
selection is stored in pixels, so scrolling or a new frame cannot disturb it.

The clipboard is not yours. Reading it is asynchronous, because the content
may belong to another program and getting it takes a round trip through the
display server. Any asynchronous call raises the question Chapter 11's
deferred calls raised: will the object that asked still exist when the
answer comes?

There are two clipboards on Linux desktops. The **clipboard** is what
Ctrl+Shift+C and Ctrl+Shift+V use. The **primary selection** holds whatever
was last selected and is pasted with the middle button. GtklabGrid uses only
the first; gtkpyte also puts every selection on the primary selection and
pastes it on a middle-click.

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
gtklab quits and the breakpoint is never hit: a capture-phase shortcut on
the window (Chapter 7 installs two) handled the key before the grid saw it. The scratch
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

## Review

1. The grid holds 5,000 lines of scrollback above a screen of 27 rows, and
   the view is at the bottom. What are the adjustment's `lower`, `upper`,
   `page_size` and `value`?
2. In the Break it, the view stays on lines 1 to 27 however far you scroll.
   Which side of the adjustment contract was broken, and why can the
   scrolled window not notice by itself?
3. New output follows the bottom only if the view was already there, but a
   key press always returns to the bottom. Why the difference?
4. Why are lines a better unit than pixels for a terminal's adjustment, and
   why is a text view's adjustment in pixels?
5. You press Return in the grid. Which byte does the grid send, which byte
   does `cat` receive, and what changed it? Why does bash at its prompt see
   something different?
6. With a grid focused, Ctrl+Shift+Q quits gtklab and the grid's key
   handler never runs. Why, and why do terminals put their own commands on
   Ctrl+Shift rather than plain Ctrl?
7. A user pastes and closes the tab before the clipboard answers. What does
   the reference held by the paste request prevent?
8. Pasting three lines ran three commands. Which code made that happen, and
   what does bracketed paste change?

**Questions for deeper thought.** If one of your tools showed a 100,000-line
device log, which unit would you choose for its adjustment, and what would
the choice make easy or hard? Neither widget uses an input method: who uses
your tools, and does that matter for them? The `commit` signal can have more
than one listener; what could a second one do in a tool for network work,
such as recording a session for replay or typing the same command into
several tabs, and what would you have to guard against?

## Exercises

1. **Page through history.** In `on_key_pressed`
   (`ch13-input/src/gtklab_grid.cpp`), make Shift+Page Up and Shift+Page
   Down move the vertical adjustment by `rows` lines and return `TRUE`
   without emitting `commit`. Done when, after `seq 1 300`, Shift+Page Up
   shows earlier lines and the scratch config's log shows no `grid key` line
   for those presses.
2. **Alt as Escape.** In `on_key_pressed`, prefix the bytes with ESC when
   Alt is held, as gtkpyte's `encode_key` does. Done when Alt+B at a bash
   prompt moves the cursor back one word.
3. **Double-click a word.** In `gtklab_grid_init`, add a `GtkGestureClick`
   for the primary button; on a press with `n_press == 2`, select the run of
   non-blank cells under the pointer. Done when a double-click highlights
   exactly one word and Ctrl+Shift+C copies just that word.
4. **The primary selection.** When a drag ends with a selection, put its
   text on `gtk_widget_get_primary_clipboard`; on a middle-button press,
   read the primary selection asynchronously, holding a reference as
   `gtklab_grid_paste_clipboard` does, and emit it through `commit`. Done
   when text selected in one tab is pasted into another by a middle-click.

