# Chapter 0 — Toolchain and the debugger

By the end of this chapter, pressing F5 in VS Code builds gtklab without
optimisation, launches it, and stops at a breakpoint, in four
configurations: plain, with the GTK Inspector, stopping on GTK critical
warnings, and under gdb with GTK's own source. Nothing in this chapter is
GTK code. It is the workbench every later chapter uses, and it is worth
getting right before there is anything to debug.

::: {.box title="In this chapter"}
- Trace a build from `meson.build` to a binary the debugger can read, and
  say what each tool in the chain contributes.
- Explain how the version-floor macros turn "runs on GTK 4.8" into a
  compile error rather than a bug report.
- Choose the right launch configuration for a problem: a crash, a layout
  question, a GTK warning, or a step into GTK itself.
- Get symbols and source for GTK on demand with debuginfod.
:::

## Concepts

### How the pieces fit

Five tools touch every build, and each does one job.

**Meson** reads `meson.build`, which says what to build: which sources, which
libraries, which flags. It does not compile anything. `meson setup` turns
that description into a build directory.

**pkg-config** is how Meson finds a library. `dependency('gtk4')` asks
pkg-config for the flags GTK needs, and pkg-config answers from a `.pc` file
installed with `libgtk-4-dev`: more than a dozen `-I` paths for GTK and the
libraries under it (Pango, GLib, Cairo, HarfBuzz), and the matching `-l`
libraries. If `pkg-config --cflags gtk4` fails, so does `meson setup`, and
the fix is the development package, not `meson.build`.

**Ninja** does the compiling. Meson writes `build.ninja` into the build
directory, and `meson compile` runs Ninja on it, which rebuilds only what
changed. You configure once and compile many times.

Meson also writes **`compile_commands.json`** beside `build.ninja`: the exact
compiler command for every source file. VS Code's IntelliSense reads it, so
the editor sees the same include paths and the same `-D` flags as the
compiler. Without it the editor does not know where GTK's headers are, and
reports errors the compiler never sees.

The **debugger** (CodeLLDB, or gdb through the C/C++ extension) needs one
more thing from the build: debug information, the tables that map machine
addresses back to source lines and variables.

![From `meson.build` to a breakpoint. Meson asks pkg-config where GTK is, writes the Ninja file and the compile commands, and the debug build gives the debugger its line tables.](diagrams/ch00-toolchain.svg)

A build directory holds one configuration. `build-asan` (Chapter 16) and
`build-floor` (Chapter 17) sit beside `build-debug` without interfering,
because nothing is generated in the source tree.

### Debug information: yours and GTK's

`--buildtype=debug` compiles with `-O0 -g`: no optimisation, full debug
information. You can check it in `compile_commands.json`, where both flags
appear in every command. Without optimisation every line of your code
exists in the binary in order and every local variable has a home, so
stepping and printing behave the way the source reads. A release build
(`-O3`, no `-g`) is faster and tells the debugger almost nothing.

The packaged GTK is built optimised and its debug information ships
separately, so out of the box the debugger shows GTK's internal frames as
`??? () at libgtk-4.so.1` (exported functions such as `g_signal_emit` keep
their names) and steps over `gtk_*` calls. That is enough for
most bugs, which are in your frame. When you need to see what GTK did with
your arguments, a debuginfod server supplies GTK's debug information and
source on demand.

### The version floor

GTK's headers mark every function with the version that introduced it and
the version that deprecated it. Four macros tell the headers which window
of versions you mean, and gtklab sets them to GTK 4.8 and GLib 2.74, the
versions Debian 12 ships:

| Macro | Effect |
|---|---|
| `GDK_VERSION_MAX_ALLOWED=4_8` | Calling anything newer than 4.8 is flagged as deprecated. |
| `GDK_VERSION_MIN_REQUIRED=4_8` | Things deprecated *after* 4.8 are not flagged, because 4.8 still needs them. |
| `GLIB_VERSION_MAX_ALLOWED`, `GLIB_VERSION_MIN_REQUIRED` | The same pair for GLib, at 2.74. |
| `-Werror=deprecated-declarations` | Turns either kind of flag into a compile error. |

The first row is the promise: call a GTK 4.10 function by accident and the
build fails on your machine, not on someone's Debian 12 jump host. The
second row is easy to overlook and just as useful. `gtk_widget_show` is
deprecated in GTK 4.10; compiled against GTK 4.14 with no floor, a call to
it is an error under `-Werror=deprecated-declarations`. With the floor it
compiles cleanly, because on 4.8 it is a normal function. The floor keeps
the compiler from pushing you toward API your oldest users do not have.

### What each launch configuration is for

Every configuration builds first, then runs the same binary with a
different debugger or environment:

| Configuration | Debugger | Adds | Reach for it when |
|---|---|---|---|
| **gtklab** | CodeLLDB | nothing | stepping your own code |
| **gtklab + GTK Inspector** | CodeLLDB | `GTK_DEBUG=interactive` | a layout, CSS or action question |
| **gtklab: stop on GTK criticals** | CodeLLDB | `G_DEBUG=fatal-criticals` | GTK printed a `CRITICAL` |
| **gtklab (gdb)** | gdb | `G_DEBUG=fatal-criticals` | you need to step into GTK |

`GTK_DEBUG=interactive` opens the **GTK Inspector** in a second window. It
shows the live widget tree, every property of the selected widget (and lets
you change them), the CSS that applies to it, its event controllers, its
actions, and the resources compiled into the program. For learning GTK it is
the single most useful tool there is, and several chapters' drills use it.

`G_DEBUG=fatal-criticals` turns a GTK `CRITICAL` warning into an abort. A
critical warning means a function was called with arguments that break its
contract: a null where an object was required, an object of the wrong type, a
widget that already has a parent. Without the variable the warning scrolls
past and the program carries on in a bad state; with it, the debugger stops
with the offending call on the stack.

From Chapter 4 the chapter directories add three more configurations:
**gtklab (scratch config)**, which keeps settings and logs under `.debug/` in
the project and turns on the book's debug output; **unit tests**, which runs
the core library's tests; and **gtklab (ASan)**, which runs the
AddressSanitizer build. The scratch configuration sets a third variable,
`G_MESSAGES_DEBUG=gtklab`, which prints the `g_debug` lines in gtklab's
code. gtklab logs under its own domain, `gtklab`, which `meson.build` sets
with `-DG_LOG_DOMAIN="gtklab"` from Chapter 5, so you see gtklab's debug
output without GTK's.

## Build

### Packages

On Debian 12, Ubuntu 23.04 or later, or ChromeOS Linux (Ubuntu 22.04 ships GTK 4.6, below the floor):

```
sudo apt install build-essential meson ninja-build pkg-config libgtk-4-dev gdb
```

On the Mac, with Homebrew:

```
brew install meson ninja pkg-config gtk4
```

The Mac has lldb with the Xcode command-line tools; gdb is Linux only.

In VS Code, install three extensions: **CodeLLDB** (the debugger the launch
configurations use), **C/C++** (IntelliSense and the gdb configuration) and
**Meson**. The repository recommends them, so VS Code offers to install them
when you open a chapter directory.

### Meson in one page

GTK and nearly everything around it builds with Meson, and so does gtklab.
If you know CMake, the mapping is short: `project()` declares the project,
`dependency()` finds a library through pkg-config, `executable()` and
`static_library()` declare targets, and a separate build directory holds
everything generated. From a chapter directory:

```
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/gtklab
```

### The first meson.build

```{.meson include="ch01-window/meson.build"}
```

`gtk_floor_args` is the version floor from the Concepts section. You can
watch it work. This is what calling `gtk_file_dialog_new`, which arrived in
GTK 4.10, produces with these flags:

```
error: 'GtkFileDialog* gtk_file_dialog_new()' is deprecated: Not available before 4.10
```

### VS Code files

Each chapter directory has a `.vscode/` folder with four files. The first
recommends the extensions:

```{.json include="ch01-window/.vscode/extensions.json"}
```

The second points IntelliSense at the compile commands:

```{.json include="ch01-window/.vscode/settings.json"}
```

The third defines the build tasks. `build-debug` configures the build
directory the first time and compiles every time; `build-asan` is used from
Chapter 16.

```{.json include="ch01-window/.vscode/tasks.json"}
```

The fourth is the one you will use most. Every configuration builds first
through `preLaunchTask`, so F5 always runs current code.

```{.json include="ch01-window/.vscode/launch.json"}
```

### Stepping into GTK

On Debian, Ubuntu and ChromeOS, gdb can download GTK's symbols and source
on demand from a debuginfod server. Set this before you start VS Code, then
use the **gtklab (gdb)** configuration:

```
export DEBUGINFOD_URLS="https://debuginfod.debian.net"
```

The first step into a GTK function pauses while the files download. On
Ubuntu, use `https://debuginfod.ubuntu.com` instead.

::: {.box title="Debugger drill"}
Once Chapter 1's `main.cpp` exists, run each launch configuration once. In
**gtklab (gdb)**, set a breakpoint on `gtk_window_present` and step in. You
should land in GTK's `gtkwindow.c`. If you get disassembly instead,
debuginfod is not reaching the debugger: check that the variable was set in
the shell that started VS Code.
:::

::: {.box title="Read in tmssh"}
`scripts/build.sh`: one script that configures, builds and tests, with
flags for a release build (`--release`), a clean build (`--clean`),
installing the toolchain (`--deps`), installing to `~/.local` (`--install`)
and running afterwards (`--run`). Note the package list it installs; Chapter
5 explains why `librsvg2-common` is on it. Then `gtk_floor_args` in its
`meson.build`, the same version floor as gtklab's.
:::

## Review

1. `meson setup` fails because it cannot find the `gtk4` dependency. Which
   tool actually failed, and what do you install?
2. Why does IntelliSense need `compile_commands.json` when the compiler
   already builds the code correctly?
3. You call a GTK 4.12 function and the build fails with "is deprecated".
   Nothing about the function is deprecated. Which macro produced the
   message, and what is it protecting?
4. Why does gtklab set `GDK_VERSION_MIN_REQUIRED` as well as
   `GDK_VERSION_MAX_ALLOWED`? What would happen to a `gtk_widget_show` call
   without it?
5. In a stack from the plain **gtklab** configuration, GTK's frames show as
   `???`. Is something misconfigured? What would you change, and when is it
   worth doing?
6. GTK printed a `CRITICAL` line and the program kept running. Which
   configuration do you run next, and what does it give you that the printed
   line does not?
7. Why keep `build-debug`, `build-asan` and `build-floor` as separate
   directories rather than reconfiguring one?

**Questions for deeper thought.** Your tool has to run on the oldest machine
in your fleet, not the newest. Which other dependencies of a tool you build
would benefit from a floor like GTK's, and how would you enforce it at
compile time? If a colleague reports a bug you cannot reproduce, which of
this chapter's configurations would you ask them to run, and what output
would you ask for?

## Exercises

1. **A fatal-warnings configuration.** In `ch01-window/.vscode/launch.json`,
   copy **gtklab: stop on GTK criticals** to a new configuration named
   **gtklab: stop on warnings** with `G_DEBUG=fatal-warnings`. Done when it
   appears in the Run and Debug list and launches the window.
2. **Compare build types.** In `ch01-window`, run
   `meson setup build-release --buildtype=release` and compile. Done when
   you can point at the difference between the two
   `compile_commands.json` files: `-O0 -g` in one, `-O3` and no `-g` in the
   other.
3. **Watch the floor.** In `on_activate` of `ch01-window/src/main.cpp`, add
   a call to `gtk_file_dialog_new()` and build. Done when you have seen the
   "Not available before 4.10" error, removed the call, and the build is
   clean again.
4. **Read pkg-config's answer.** Run `pkg-config --cflags gtk4` and find
   each `-I` path in the command for `main.cpp` in
   `build-debug/compile_commands.json`. Done when you can say which library
   each of three paths belongs to.
