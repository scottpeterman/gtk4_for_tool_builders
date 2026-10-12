# Chapter 4 — Keep GTK out of the logic

Before gtklab shows a single session, this chapter writes the code that reads
sessions, builds ssh command lines and loads settings, and tests it, without
a window. It comes before the list view chapters on purpose. Chapters 5 and 6
display this model; they do not own it.

## The idea: a core library with no display

A desktop tool has two kinds of code. Some of it needs a display: windows,
widgets, the clipboard, drawing. Most of it does not: parsing a file,
validating it, turning a session into a command line, clamping a setting to a
sane range. Put the second kind in a static library that never includes
`gtk/gtk.h`, and three things follow.

The logic gets tests that run in milliseconds, with no window and no main
loop, so most bugs are found by a test runner instead of by clicking. The
logic can be stepped through in a debugger without GTK on the stack at all.
And the boundary is enforced by the build: the core target is not given GTK,
so an accidental `#include <gtk/gtk.h>` in it does not compile.

GLib is allowed in the core. It is not a GUI library; it is GTK's portable C
runtime, and it brings things a tool needs anyway: `GKeyFile` for INI-style
files, `g_file_get_contents` and `g_file_set_contents`, path helpers, and
UTF-8 functions.

![The build targets from this chapter on. The core library depends on GLib only; the application adds GTK; the tests link only the core.](diagrams/core-split.svg)

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

`Settings` replaces Checkpoint A's ad-hoc key file. Later chapters add
fields to it, and the struct is the only place their defaults live:

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

::: {.box title="Check yourself"}
What is allowed in the core and what is not? Why does `ssh_argv` take the
shell as a parameter? How does the build stop GTK from creeping into the
core?
:::
