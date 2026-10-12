# Chapter 14 — Swapping in gtkpyte

GtklabGrid has taught its lesson. This chapter replaces it with gtkpyte, a
real terminal emulator for GTK 4 built on a C++ port of the pyte emulation
core. Colour, full-screen programs, mouse reporting, wide characters,
bracketed paste: all of it arrives at once. The chapter also adds font zoom
and per-tab session logging, and it is where the main-loop priority from
Chapter 11 is measured.

::: {.box title="Recall"}
What are the three phases an event passes through (Chapter 9)? Which two
things stop a large `cat` from freezing the window (Chapter 11)? Which signal
tells the pty that the window changed size (Chapter 12)?
:::

::: {.box title="In this chapter"}
- Replace one widget with another behind the same signals, without touching
  the session that feeds it.
- Explain what a terminal emulator does with the bytes that GtklabGrid
  skipped: parsing, modes, replies, a second screen.
- Predict where a mouse press or a wheel step goes, from the modes the
  program in the terminal has set.
- Vendor a third-party library into a Meson build and keep its headers from
  shadowing yours.
- Tap a byte stream for logging without disturbing it, and measure what a
  source priority costs.
:::

## Concepts

### The same shape, a real engine

gtkpyte's terminal widget owns a screen and nothing else. It has no pty, no
shell and no ssh: bytes go in through `gtkpyte_terminal_feed`, and bytes come
out through its `commit` signal. Its signals match GtklabGrid's on purpose:

```{.c include="ch14-gtkpyte/gtkpyte/src/gtkpyte-terminal.h" from="* Signals" to="*       A selection appeared or went away -- for enabling a Copy menu item."}
```

So the swap is wiring: the same `PtySession` from Chapter 11 feeds the new
widget, and the same two signals go back the other way. That is the payoff
of Chapters 12 and 13 refusing to let the widget know about ptys.

![Data flow for one tab. Bytes from the child go through the session's log tap into the terminal; keys and pastes leave as `commit`; a new size leaves as `resized`.](diagrams/terminal-dataflow.svg)

### What an emulator does that the grid did not

GtklabGrid recognised escape sequences only to throw them away. A program
running under `TERM=xterm-256color` relies on them for everything beyond
plain text: the terminfo entry for `xterm-256color` says that the sequence
to reset attributes is `ESC ( B ESC [ m` and that entering a full-screen
program sends `ESC [ ? 1049 h`. An emulator has to act on all of that.

**It parses with a state machine.** pyte's `Stream` reads one byte at a
time and remembers which state it is in between calls, so a sequence split
across two pty reads needs no special handling: the next `feed` simply
continues where the last one stopped. GtklabGrid's approach, buffering an
incomplete sequence and scanning it again, works only for the shapes it
knows. Its rule that any other `ESC x` is two bytes would print the `B` of
the three-byte `ESC ( B` above.

![pyte's parser states. Every byte moves the machine; only the Ground state draws text. A CSI sequence collects numeric parameters until a final byte names the command.](diagrams/ch14-stream.svg)

**It keeps a screen of cells, not characters.** Each cell has a character
and attributes: colours, bold, underline. The `CSI` commands move the
cursor, erase, insert and delete lines and characters, set a scrolling
region, and change attributes (`CSI ... m`). The widget paints from those
cells.

**It answers.** Some sequences are questions. A program can ask where the
cursor is (`CSI 6 n`) or what kind of terminal this is (`CSI c`), and wait
for the reply. pyte produces the reply, and gtkpyte sends it out through the
same `commit` signal as keystrokes. Its header puts the consequence plainly:
drop `commit`, and applications that query the terminal will hang waiting.

**It keeps state the program sets.** Some sequences change how the terminal
behaves rather than what it shows. They are the private modes, set with
`CSI ? n h` and reset with `CSI ? n l`, and pyte reports each one to the
widget:

| Mode | Name | What it changes | Kept in |
| --- | --- | --- | --- |
| 1 | cursor keys (DECCKM) | arrows send `ESC O A` instead of `ESC [ A` | `KeyModes::application_cursor` |
| 25 | cursor visible | whether the cursor is drawn | `State::cursor_visible` |
| 47, 1047, 1049 | alternate screen | which buffer is shown | pyte's `Screen` |
| 1000, 1002, 1003 | mouse tracking | which mouse events are reported | `MouseModes::tracking` |
| 1006 | SGR mouse encoding | report format, no coordinate limit | `MouseModes::sgr` |
| 1007 | alternate scroll | the wheel sends arrows on the alternate screen | `MouseModes::alternate_scroll` |
| 2004 | bracketed paste | pastes are wrapped in `ESC [200~` and `ESC [201~` | `State::bracketed_paste` |

This is why Chapter 13's key table had a column that depended on state: the
terminfo entry gives the Up key as `ESC O A`, the cursor-key-mode form, and
a program that turns that mode on expects it.

### Two screens

A shell writes line after line, and what scrolls off the top goes into
scrollback. A full-screen program such as `vim` or `htop` instead redraws a
fixed screen in place, and when it exits, the shell's screen should come back
as it was. Mode 1049 provides that: on entry, pyte saves the cursor,
switches to a second buffer and clears it; on exit, it switches back and
restores the cursor. The alternate buffer keeps no scrollback, because a
redrawn screen has no history worth keeping.

The second screen changes what the wheel means. gtkpyte decides on every
scroll event:

| Situation | The wheel |
| --- | --- |
| The program asked for mouse reports, no Shift | is sent to it as button presses (64 up, 65 down) at the pointer's cell |
| Alternate screen, no mouse reports, mode 1007 on (the default) | sends Up or Down arrow keys, so `less` and `man` scroll |
| Normal screen | scrolls the scrollback, three lines per notch |

### Who gets the mouse

Mouse reporting raises Chapter 9's question again: two controllers on one
widget want the same press. Selection wants it to start a drag; a program
that asked for the mouse wants it as a report. gtkpyte settles it with
propagation. A drag gesture for every button runs in the capture phase, ahead
of the selection gestures. When the program has asked for the mouse and
Shift is not held, it claims the sequence: the press, and the motion and
release when the mode reports them, go to the program as report bytes
through `commit`. Otherwise it denies the
sequence, and the press continues to selection untouched.

![One press, two possible owners. The capture-phase report gesture claims the press when the program has asked for the mouse; otherwise it denies it and selection gets it.](diagrams/ch14-mouse.svg)

Shift as the override is xterm's convention: it lets the user select text
even inside a program that has taken the mouse. The widget also changes its
pointer from the text I-beam to an arrow while reporting is on, so the user
can see who owns the mouse.

### Vendoring a widget

gtkpyte arrives as source copied into the tree, not as a system package.
That is **vendoring**, and it trades one set of problems for another. You
get a fixed version that builds the same everywhere, with no package to
install; you take on keeping it up to date. The `VENDORED.md` that comes
with the copy records
the upstream commit the copy came from, lists which files came from where,
and sets the rule that nothing in the copy is edited in place: fixes go
upstream first and are copied down. Two more costs come with it. The licence
travels with the code, so gtklab becomes GPL. And the library's include
directory becomes yours: a header it ships can shadow one of the system's, as
the Build section finds out with `pty.h`.

## Build

### Vendoring gtkpyte

Copy tmssh's `gtkpyte/` directory (its sources, its `pyte/` core, its
`meson.build` and `VENDORED.md`) into gtklab, and copy tmssh's
`subprojects/utf8proc.wrap` and `subprojects/packagefiles/utf8proc/`. The
first time the build is configured, Meson fetches utf8proc 2.11.3 from
GitHub; after that it builds offline.

gtkpyte is licensed GPL-3.0-or-later, so from this chapter on, gtklab is too.

### meson.build

gtkpyte's `meson.build` expects a variable called `gtk_dep` and provides
`gtkpyte_dep`:

```{.meson include="ch14-gtkpyte/meson.build" from="# The terminal widget, vendored from gtkpyte" to="subdir('gtkpyte')"}
```

Then a problem that only shows up in a real build. gtkpyte's source
directory, which is on the include path of anything that uses `gtkpyte_dep`,
contains its own `pty.h`. With it on the path, `#include <pty.h>` in
`pty_session.cpp` finds gtkpyte's file instead of the system's, and the build
fails with `'forkpty' was not declared in this scope`. The fix is to build
the pty code as its own small library, which does not use `gtkpyte_dep`:

```{.meson include="ch14-gtkpyte/meson.build" from="# Our pty code is built on its own" to="dependencies: [gtk, core_dep, util, gtkpyte_dep])"}
```

### The wiring

```{.cpp include="ch14-gtkpyte/src/app_window.cpp" fn="void AppWindow::start_tab("}
```

The child now runs with `TERM=xterm-256color`, because the terminal really
is one. The log tap sits in the output handler, so the log sees the raw bytes
before the terminal does.

### Zoom

The zoom from Chapter 9 becomes one font size for every tab, from Ctrl+=,
Ctrl+- and Ctrl+0, and from Ctrl+wheel, saved through the debounced save:

```{.cpp include="ch14-gtkpyte/src/app_window.cpp" fn="void AppWindow::set_font_size("}
```

```{.cpp include="ch14-gtkpyte/src/app_window.cpp" fn="void AppWindow::apply_font("}
```

### Session logs

A `SessionLog` opens a file under `$XDG_DATA_HOME/gtklab/logs/`, appends raw
bytes, and writes a footer when it is destroyed:

```{.cpp include="ch14-gtkpyte/src/session_log.h"}
```

The tab owns its log as a `std::unique_ptr<SessionLog>`, declared after the
session, so that the log closes first when a tab is destroyed. Logging is a
stateful action, `win.log` (Ctrl+Shift+L), whose check mark must follow the
current tab. The action creates or drops the current tab's log and shows a
dot in its tab label:

```{.cpp include="ch14-gtkpyte/src/app_window.cpp" fn="void AppWindow::act_log("}
```

and switching tabs moves the check mark:

```{.cpp include="ch14-gtkpyte/src/app_window.cpp" fn="void AppWindow::sync_log_action("}
```

Copy, Paste and Select All now call `gtkpyte_terminal_copy_clipboard`,
`gtkpyte_terminal_paste_clipboard` and `gtkpyte_terminal_select_all`.

## Done when

Colour output, `htop` and `vim` work in a local tab; zoom changes the font in
every tab and survives a restart; Ctrl+Shift+L shows the log dot and writes
a log file.

![Colour with the log dot in the tab label; `htop`; `vim` after a mouse click on line 12.](images/ch14-gtkpyte.png)

::: {.box title="Debugger drill"}
Run `vi -u NONE -c "set mouse=a ruler"` on a file of a hundred lines and
click a line. The ruler jumps to it; in the recorded run, `12,2`. Now set a
breakpoint in gtkpyte's `on_report_begin` and click again: the gesture claims
the press and sends it to vim as a mouse report. Quit vim and click at the
shell prompt: the same gesture denies the press, and selection gets it
instead.
:::

::: {.box title="Break it"}
In `pty_session.cpp`, go back to `g_unix_fd_add`, which watches at the
default priority, and `cat` a large file. This was measured with a 67 MB
file, taking a screenshot every second and comparing them:

| Read watch priority | Screens that changed, 6 one-second samples | Time for the whole `cat` |
| --- | --- | --- |
| `G_PRIORITY_DEFAULT` (`g_unix_fd_add`) | 1 change in 6 seconds | 17.9 s |
| `G_PRIORITY_DEFAULT_IDLE` | changed every second | 16.5 s |

At the default priority the terminal is frozen on one screen for seconds at a
time while the bytes pour through. At idle priority it repaints throughout,
and the whole run takes about the same time. The priority costs nothing; it
only decides whether the redraw gets a turn. The measurements were made under
Xvfb with software rendering, where a frame is expensive; the mechanism is
the same on a real GPU.
:::

## Common mistakes

**Linking both `pty.h`s.** Any library with a `pty.h` on its public include
path shadows the system header for everything that uses it. Keep your pty
code out of that target.

**Feeding the terminal outside the read budget.** Feed it inside
`on_readable`, chunk by chunk; the budget is what keeps the loop moving.

**Logging after the terminal.** Tap the bytes before `gtkpyte_terminal_feed`,
so the log has what the child sent, not what the screen became.

::: {.box title="Read in tmssh"}
`start_tab`, `act_log`, `sync_log_action`, and `LogCleaner` in
`src/session_log.h`: tmssh strips escape sequences out of its logs so they
read as plain text, where gtklab keeps raw bytes. Then
`gtkpyte::PtySession`, which is Chapter 11's `PtySession` with one important
addition: a write queue, so that a long paste into a slow child is never cut
short.
:::

## Review

1. Why does swapping GtklabGrid for gtkpyte not change `PtySession` at all?
2. A pty read ends in the middle of `ESC [ 3 8 ; 5`. What does pyte do with
   the rest when the next read arrives, and why is that simpler than
   GtklabGrid's approach?
3. A program sends `CSI 6 n` and waits. What does it get back, by which
   path, and what happens if nothing is connected to `commit`?
4. While `vim` runs, there is no scrollback to wheel through. Why? What does
   the wheel do in `vim` with `set mouse=a`, and what without it?
5. Inside `vim` with the mouse on, how does a user still select text, and
   which code makes that possible?
6. Why does the pty code build as its own library?
7. Moving the read watch from `G_PRIORITY_DEFAULT` to
   `G_PRIORITY_DEFAULT_IDLE`: what did it cost, and what did it buy?
8. The log tap runs before `gtkpyte_terminal_feed`. What does the log file
   therefore contain, and why is that different from what the screen shows?

**Questions for deeper thought.** For a tool you hand to colleagues, would
you vendor a widget like gtkpyte or depend on a system package, and what
would you write down either way? gtklab's `PtySession::write` drops what the
kernel does not accept, where gtkpyte's session queues it: when would that
difference bite a network engineer, for example pasting a long
configuration into a slow console? For an audit of changes made on network
devices, do you want raw logs, cleaned logs, or both?

## Exercises

1. **Copy only when there is something to copy.** In
   `ch14-gtkpyte/src/app_window.cpp`, connect each terminal's
   `selection-changed` signal in `AppWindow::start_tab` to a function that
   enables `win.copy` only when the current tab's terminal has a selection,
   and call the same function where `on_switch_page` syncs the log action.
   Done when the Copy menu item is greyed out until text is selected, and
   follows the current tab.
2. **A colour scheme setting.** Add a `color_scheme` field to `Settings`
   (`ch14-gtkpyte/src/core/settings.h` and `settings.cpp`, with a doctest
   case), apply it next to the font in `AppWindow::apply_font` with
   `gtkpyte_terminal_set_color_scheme`, and offer the names from
   `gtkpyte_terminal_list_color_schemes` as a stateful action in the menu.
   Done when a chosen scheme applies to every tab and survives a restart.
3. **A write queue.** In `PtySession::write`
   (`ch14-gtkpyte/src/pty_session.cpp`), keep the bytes the kernel does not
   accept and write them from a `G_IO_OUT` watch, removed in the destructor
   like the others; gtkpyte's `PtySession::send` and `drain` show one way.
   Done when pasting a 1 MB text file with Unix line endings into `wc -c`
   (then Ctrl+D) reports the same count as `wc -c` on the file.
4. **Readable logs.** Give `SessionLog` an option to strip escape sequences
   before writing, with a small state machine that keeps its state between
   calls, as in the parser figure. Done when the log of a session that ran
   `ls --color` reads as plain text in `less`, and a sequence split across
   two writes leaves no stray bytes. tmssh's `LogCleaner` is a reference.


# Checkpoint D — A usable terminal {.unnumbered}

**Spec.** Real local and ssh tabs; zoom persisted; per-tab logging with
Ctrl+Shift+L and a dot in the tab label; a tab that closes itself when its
session exits.

**Done when.** `cat` of a 50 MB file leaves the window repainting; `htop` and
`vim` render and take the mouse; and every gtklab shortcut works with a
terminal focused. All three were checked against the Chapter 14 code, which
is the reference for this checkpoint; the 67 MB `cat` was also logged, and
the log file held all of it.

**Reference.** `ch14-gtkpyte/`.
