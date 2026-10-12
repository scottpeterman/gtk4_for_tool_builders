# Chapter 0 — Toolchain and the debugger

By the end of this chapter, pressing F5 in VS Code builds gtklab without
optimisation, launches it, and stops at a breakpoint, in four
configurations: plain, with the GTK Inspector, stopping on GTK critical
warnings, and under gdb with GTK's own source. Nothing in this chapter is
GTK code. It is the workbench every later chapter uses, and it is worth
getting right before there is anything to debug.

## Packages

On Debian 12, Ubuntu 22.04 or later, or ChromeOS Linux:

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

## Meson in one page

GTK and nearly everything around it builds with Meson, and so does gtklab.
If you know CMake, the mapping is short: `project()` declares the project,
`dependency()` finds a library through pkg-config, `executable()` and
`static_library()` declare targets, and a separate build directory holds
everything generated. You configure once and compile many times:

```
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/gtklab
```

`--buildtype=debug` turns optimisation off and debug information on, so the
debugger steps through your code line by line and every variable is
visible. You can keep several build directories side by side; Chapter 16
adds `build-asan` for AddressSanitizer and Chapter 17 adds `build-floor` for
the oldest supported GTK.

Meson also writes `compile_commands.json` into the build directory. That file
is what lets VS Code's IntelliSense find GTK's headers and jump into them.

## The first meson.build

```{.meson include="ch01-window/meson.build"}
```

The `gtk_floor_args` deserve a closer look, because they hold the whole book
to a promise. GTK's headers mark every function with the version that
introduced it. `GDK_VERSION_MAX_ALLOWED` tells the headers that you intend to
run on GTK 4.8, so any newer function produces a deprecation warning, and
`-Werror=deprecated-declarations` turns that warning into an error. If you
call a GTK 4.10 function by accident, you find out when you compile, not when
someone runs your tool on Debian 12. The GLib pair does the same for GLib
2.74, the version Debian 12 ships with GTK 4.8.

You can watch it work. This is what calling `gtk_file_dialog_new`, which
arrived in GTK 4.10, produces with these flags:

```
error: 'GtkFileDialog* gtk_file_dialog_new()' is deprecated: Not available before 4.10
```

## VS Code files

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

From Chapter 4 the chapter directories add three more configurations:
**gtklab (scratch config)**, which keeps settings and logs under `.debug/` in
the project and turns on the book's debug output; **unit tests**, which runs
the core library's tests; and **gtklab (ASan)**, which runs the
AddressSanitizer build.

## Three environment variables

Most GTK debugging is done with three environment variables, and the launch
configurations set them for you.

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

`G_MESSAGES_DEBUG=gtklab` prints the `g_debug` lines in gtklab's code. gtklab
logs under its own domain, `gtklab`, which `meson.build` sets with
`-DG_LOG_DOMAIN="gtklab"` from Chapter 5, so you see gtklab's debug output
without GTK's.

## Stepping into GTK

GTK installed from packages has no debug information, so stepping into a
`gtk_*` call steps over it. On Debian, Ubuntu and ChromeOS, gdb can download
symbols and source on demand from a debuginfod server. Set this before you
start VS Code, then use the **gtklab (gdb)** configuration:

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
