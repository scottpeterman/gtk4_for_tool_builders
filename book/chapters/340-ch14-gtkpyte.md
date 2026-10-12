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

## The idea: the same shape, a real engine

gtkpyte's terminal widget owns a screen and nothing else. It has no pty, no
shell and no ssh: bytes go in through `gtkpyte_terminal_feed`, and bytes come
out through its `commit` signal. Its signals match GtklabGrid's on purpose:

```{.c include="ch14-gtkpyte/gtkpyte/src/gtkpyte-terminal.h" from="* Signals" to="*       A selection appeared or went away -- for enabling a Copy menu item."}
```

So the swap is wiring: the same `PtySession` from Chapter 11 feeds the new
widget, and the same two signals go back the other way.

![Data flow for one tab. Bytes from the child go through the session's log tap into the terminal; keys and pastes leave as `commit`; a new size leaves as `resized`.](diagrams/terminal-dataflow.svg)

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

::: {.box title="Check yourself"}
Why does swapping the widget not change `PtySession`? Why does the pty code
build as its own library? What did the priority change cost, and what did it
buy?
:::

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
