# Chapter 4 — Keep GTK out of the logic

Before gtklab shows a single session, this chapter writes the code that reads
sessions, builds ssh command lines and loads settings, and tests it, without
a window. It comes before the list view chapters on purpose. Chapters 5 and 6
display this model; they do not own it.

::: {.box title="In this chapter"}
- Decide, function by function, whether code belongs in a GTK-free core or
  in the GTK layer.
- Build a core static library that the build itself keeps free of GTK.
- Write GTK callbacks as adapters: turn the event into a core call, then
  refresh the view from the core.
- Test parsing, command building and settings with doctest, with no window
  and no main loop.
:::

## Concepts

### Two kinds of code

A desktop tool has two kinds of code. Some of it needs a display: windows,
widgets, the clipboard, drawing. Most of it does not: parsing a file,
validating it, turning a session into a command line, clamping a setting to a
sane range. Put the second kind in a static library that never includes
`gtk/gtk.h`, and three things follow.

The logic gets tests that run in milliseconds, with no window and no main
loop, so most bugs are found by a test runner instead of by clicking. The
logic can be stepped through in a debugger without GTK on the stack at all.
And the same code is ready for the next front end a tool tends to grow: a
command-line mode, a script, a scheduled job. None of them should have to
open a window to build an ssh command line.

The test for which side a function belongs on is what it needs to know, not
how long it is:

| Core (no GTK) | GTK layer |
|---|---|
| parse and validate `sessions.ini` | build the sidebar and the notebook |
| build the argv for a session | connect signals, decide what a click does |
| filter sessions by search text | read the search entry's text |
| load, clamp and save settings | read the window's size in `close-request` |
| report errors as values | show an error to the user |

Each row is one job split at the boundary. The right column asks GTK a
question or shows an answer; the left column computes the answer.

### Dependencies point one way

The GTK layer depends on the core. The core never depends on the GTK
layer, and the build enforces it: the core target is not given GTK, so an
accidental `#include <gtk/gtk.h>` in it does not compile. A rule the
compiler checks survives deadlines and new contributors; a rule in a
comment does not.

![The build targets from this chapter on. The core library depends on GLib only; the application adds GTK; the tests link only the core.](diagrams/core-split.svg)

GLib is allowed in the core. It is not a GUI library; it is GTK's portable C
runtime, and it brings things a tool needs anyway: `GKeyFile` for INI-style
files, `g_file_get_contents` and `g_file_set_contents`, path helpers, and
UTF-8 functions. The line is drawn at the display, not at GNOME.

### The callback is an adapter

GTK is event-driven, and a callback is how it tells you something happened.
The mistake is to let the callback become the place where the work lives: it
reads an entry, parses the text, writes a file, updates three labels. Each
feature adds a reason to change it, and soon nothing useful can happen
without a window.

The alternative is a callback that only translates. It turns the event into
a call on the core, then refreshes the view from what the core now holds.
It knows both GTK and the core; the core knows nothing of GTK.

![A callback as an adapter, from this chapter's Isolated lab. The click changes the model, then the view is redrawn from the model; the tests call the same model with no GTK at all.](diagrams/ch04-adapter.svg)

Two kinds of state follow from this. **Model state** (the sessions, the
settings, a count) stays meaningful with no window open. **View state** (a
label's text, a list row) is a presentation of it. The model is the source
of truth; a label is output, never storage. A callback that parses a label
to find out what the count was has the arrow backwards.

### What makes core code testable

A core function is easy to test when everything it depends on comes in
through its parameters and everything it produces goes out through its
return value. Hidden inputs (an environment variable, the current
directory, the clock) make a test pass on one machine and fail on another;
hidden outputs (a dialog, a `g_warning`) make a failure invisible to the
test. The Build section shows two places where gtklab's core was shaped by
this rule.

Core tests and GUI checks answer different questions. A core test asks
whether the behaviour is right: does a session on port 2222 produce
`-p 2222`? A check through the window asks whether GTK is wired to that
behaviour: does a double-click start the right session? The first kind is
cheap, so write many; the second is slow, so write few, and only for the
wiring.

### How the rest of the book uses the core

Chapters 5 and 6 display the core's `Folder` and `Session` values without
owning them: a small GObject wrapper holds a copy so a GTK list can show it.
From this chapter on, the window loads and saves `Settings` through the core;
the fields later chapters need, such as the font size and the terminal
theme, are already declared in this chapter's struct, with their defaults. tmssh has the same shape at a larger size:
its core library holds session parsing, the ssh command builder, settings,
themes and the session-log cleaner, each with tests.

## Build

### The session model

The interface, complete:

```{.cpp include="ch04-core/src/core/session.h"}
```

Two design choices are worth copying. Errors come back as a string in an out
parameter, with an empty result, rather than as exceptions crossing into C
callbacks later. And `ssh_argv` takes the shell as a parameter instead of
reading `$SHELL` itself, so a test does not depend on the machine running it.

The parser is built on `GKeyFile`. Note how a missing `port` key and a `port`
key that is not a number are told apart by the `GError` code:

```{.cpp include="ch04-core/src/core/session.cpp" fn="std::vector<Folder> parse_sessions("}
```

The command builder is the piece most worth testing, because a wrong
argument in it produces an ssh that connects to the wrong place:

```{.cpp include="ch04-core/src/core/session.cpp" fn="std::vector<std::string> ssh_argv("}
```

tmssh reads YAML in PathfinderSSH's format with yaml-cpp. gtklab uses
`GKeyFile` so that its core needs nothing beyond GLib; the shape of the code
is the same.

### Settings

`Settings` replaces Checkpoint A's ad-hoc key file. Later chapters use
its fields, and the struct is the only place their defaults live:

```{.cpp include="ch04-core/src/core/settings.h"}
```

Two details in `settings.cpp` matter for a tool people hand-edit. Values are
clamped on load, so a settings file that says `width=10` gives a usable
window rather than a ten-pixel one. And saving goes through
`g_file_set_contents`, which writes a temporary file and renames it over the
old one, so a crash in the middle of a save never leaves half a file.

### Tests

doctest is a single header. Copy `doctest.h` into `tests/` rather than
fetching it when the build is configured: the build then works offline,
which matters on a jump host. Eleven tests cover the core; here are three:

```{.cpp include="ch04-core/tests/test_session.cpp" fn="TEST_CASE(\"ssh argv: custom port\")"}
```

```{.cpp include="ch04-core/tests/test_session.cpp" fn="TEST_CASE(\"parse: malformed files return an error, not a crash\")"}
```

```{.cpp include="ch04-core/tests/test_settings.cpp" fn="TEST_CASE(\"settings: hand-edited nonsense is clamped\")"}
```

### meson.build

```{.meson include="ch04-core/meson.build"}
```

Note that the core library itself is given the include directory, not only
the targets that depend on it. `session.cpp` includes `core/session.h`, so
the core needs `src/` on its include path too; the first version of this
file left it out and failed with `core/settings.h: No such file or
directory`.

### The window uses the core

`main.cpp` now loads settings through the core, creates a default sessions
file on first run, parses it and prints what it found:

```{.cpp include="ch04-core/src/main.cpp" fn="static void ensure_sessions_file("}
```

## Done when

`meson test -C build-debug` passes, and the first run creates
`~/.config/gtklab/sessions.ini` with one Local folder holding localhost, and
prints `folder Local: 1 sessions`.

::: {.box title="Debugger drill"}
Run the **unit tests** launch configuration. It passes `-tc=*argv*`, so four
of the eleven tests run. Set a breakpoint inside `ssh_argv`. When it stops,
look at the stack: `main`, doctest's runner, your test, `ssh_argv`. No
window, no main loop, no GTK. Compare it with the stack from Chapter 1.
:::

::: {.box title="Break it"}
Add `#include <gtk/gtk.h>` to the top of `session.cpp` and build. The core
does not compile: `fatal error: gtk/gtk.h: No such file or directory`. The
core target was never given GTK, so the boundary holds by construction. That
error is the split doing its job.
:::

## Common mistakes

**A GTK type in a core header.** One `GtkWidget *` in `session.h` and every
test needs GTK. Keep core headers to standard C++ and GLib.

**Testing through the window.** If the only way to check the ssh command is
to click a session, every change costs a minute. Test it in the core in a
millisecond.

**Throwing across C callbacks.** An exception that escapes a GTK callback
unwinds through C frames that were not built for it. Report errors as values.

::: {.box title="Read in tmssh"}
`tmssh_core` in tmssh's `meson.build`: the library holding `sessions.cpp`,
`ssh_command.cpp`, `settings.cpp`, `themes.cpp` and the session-log cleaner.
Then `tests/test_sessions.cpp` and `tests/test_ssh_command.cpp`.
:::

## Review

1. A function formats a session as `user@host:port` for the sidebar's
   tooltip. Core or GTK layer? What about the function that sets the
   tooltip?
2. What stops an `#include <gtk/gtk.h>` from creeping into `session.cpp`,
   and why is that better than a comment saying "no GTK here"?
3. GLib is allowed in the core and GTK is not. Where exactly is the line
   drawn, and why there?
4. `ssh_argv` takes the shell as a parameter instead of reading `$SHELL`.
   What would go wrong in the tests if it read the environment itself?
5. Why does the core report a bad sessions file as a string in an out
   parameter rather than by throwing?
6. In the lab, `on_increment` changes the model and then calls
   `refresh_view`. What would be wrong with reading the number back from the
   label's text and adding one?
7. Which questions can a core test answer that a click-through test cannot,
   and which can only the click-through answer?
8. Chapter 5 shows sessions in a list. Why does it wrap them in a GObject
   instead of making `Session` itself a GObject?

**Questions for deeper thought.** Take a tool you already have that only
works with its window open. Which of its functions would move to a core
library, and which piece would be hardest to separate? If the same core had
to serve a command-line mode tomorrow, what in its interface would you
change today?

## Exercises

1. **An identity file.** Add an `identity` field to `Session` in
   `ch04-core/src/core/session.h`, read it in `parse_sessions`, and make
   `ssh_argv` add `-i <path>` when it is set. Add a test case in
   `tests/test_session.cpp`. Done when `meson test -C build-debug` passes
   with the new test and the existing eleven.
2. **Search the folder name.** Change `filter_sessions` so text matching a
   folder's name keeps every session in that folder. Write the failing test
   first. Done when the new test passes and
   "filter: case-insensitive on name, host and user; empty folders dropped"
   still does.
3. **A clamped setting.** Add a `scrollback` field to `Settings` (default
   10000, clamped to 100 to 100000) in `settings.h`, `parse_settings` and
   `settings_to_text`, and extend
   "settings: hand-edited nonsense is clamped". Done when a file saying
   `scrollback=5` loads as 100.
4. **Print the commands.** In `on_activate` in `ch04-core/src/main.cpp`,
   print each session's `ssh_argv`, joined by spaces, passing
   `g_getenv("SHELL")` (or an empty string when it is unset). Done when the
   first run prints your shell's path for the localhost session.

## Isolated lab

gtklab's Chapter 4 split is real but busy: two core files, eleven tests,
settings on disk. The lab isolates the one idea underneath it, the callback
as an adapter, in a program small enough to hold in your head. A
`CounterModel` knows how to count and describe itself and nothing else; the
window knows how to show a label and two buttons; two callbacks connect
them. It is a separate program so that the boundary is the only thing on
the screen.

The model, header-only:

```{.cpp include="labs/lab04-boundary/src/core/counter_model.h"}
```

The window. `WindowState` holds the model and a borrowed pointer to the
label, and the window owns the state through `g_object_set_data_full`, the
pattern from Chapter 2:

```{.cpp include="labs/lab04-boundary/src/main.cpp"}
```

The test, which links no GTK:

```{.cpp include="labs/lab04-boundary/tests/test_counter.cpp"}
```

```{.meson include="labs/lab04-boundary/meson.build" from="# The core is header-only here." to="executable('lab-tests'"}
```

Build, run and test from `labs/lab04-boundary`:

```
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/lab
meson test -C build-debug
```

In VS Code, **unit tests (no GTK)** runs `lab-tests` under the debugger.

| Stop at | What it shows |
|---|---|
| `on_increment` | the click arriving as a signal; step into `increment()` and the stack below the model is the adapter, then GTK |
| `refresh_view` | the label's text derived from `model.description()`; `gtk_label_set_text` copies it, so the `std::string` can go |
| the lambda in `on_activate` that deletes `WindowState` | it runs only when the window closes, called from `g_datalist_clear` inside `g_object_unref` |
| `main` in `test_counter.cpp` (**unit tests (no GTK)**) | the same `CounterModel` calls with no GTK anywhere on the stack |

Experiments:

1. **Break the boundary.** Add `#include <gtk/gtk.h>` to the top of
   `counter_model.h` and build. `lab` still compiles, because it is given
   GTK; `lab-tests` fails with `fatal error: gtk/gtk.h: No such file or
   directory`. The test target is the guard.
2. **Add a rule to the model.** Add `decrement()`, and decide in the model
   whether the count may go negative. Add a Decrement button whose handler
   has the same two lines as `on_increment`, and an `assert` in the test.
   The callback should not need an `if`.
3. **Store state in the view.** Rewrite `on_increment` to parse the number
   out of the label's text and write it back, without the model. Then ask
   how a test would check it, and how Reset would work if the label's
   wording changed.
4. **Watch the state die.** Put a breakpoint on the delete lambda, click
   Increment three times and close the window. Read the stack from the
   bottom: the window closing, the window's last reference going, its data
   cleared, your lambda.

![Lab 4 after three clicks on Increment.](images/lab04-boundary.png){width=55%}
