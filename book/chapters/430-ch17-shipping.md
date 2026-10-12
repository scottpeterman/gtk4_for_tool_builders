# Chapter 17 — Holding the floor, and shipping

Every chapter has compiled against the GTK 4.8 API, because the version
macros from Chapter 0 make anything newer a compile error. That proves less
than it seems. This chapter builds gtklab against a real GTK 4.8, adds the
install rules a desktop needs, and finishes with the book's last exercise:
a second tool of your own.

::: {.box title="Recall"}
Which macros hold the code to the GTK 4.8 API (Chapter 0)? Why does
Chapter 10 use `GtkFileChooserNative` rather than `GtkFileDialog`? What did
the ASan build need turned off, and why (Chapter 16)?
:::

## The idea: the floor is a machine, not a macro

`GDK_VERSION_MAX_ALLOWED` catches a call to a function that is newer than
4.8. It cannot catch:

- a change in **behaviour** between 4.8 and the GTK you develop on, such as a
  widget that measures differently or a signal emitted in a different order;
- a **dependency** that is too new, such as a GLib function newer than 2.74
  that slipped in through a header without its own version macros;
- a **bug** in 4.8 that newer versions fixed.

Only building and running on the floor itself catches these. The **floor**
is the oldest platform you promise to support; for this book, Debian 12, with
GTK 4.8 and GLib 2.74. Build there before every release, and run the program,
not just the tests.

## Build

### On Debian 12

GTK 4.8 is Debian 12's system GTK, so a Debian 12 machine or container
builds gtklab with no special steps. From a checkout, with Docker installed,
one command does it:

```
docker run --rm -v "$PWD":/src -w /src debian:12 bash -c "apt-get update && apt-get install -y build-essential meson ninja-build pkg-config libgtk-4-dev git && meson setup build-floor && meson compile -C build-floor && meson test -C build-floor"
```

(This line was not run for the book: the machine it was written on had no
Docker daemon. The next route is what was run.)

### GTK 4.8 in a prefix

Without Debian 12, build GTK 4.8.3 into a directory of its own. The options
turn off everything gtklab does not use, which keeps the build to a few
minutes and its dependencies to the ones a GTK 4 development machine already
has:

```
git clone --depth 1 --branch 4.8.3 https://github.com/GNOME/gtk.git gtk-4.8.3
meson setup gtk-4.8.3/build gtk-4.8.3 --prefix=$HOME/floor/gtk48 --buildtype=release -Dx11-backend=true -Dwayland-backend=false -Dbroadway-backend=false -Dmedia-gstreamer=disabled -Dmedia-ffmpeg=disabled -Dprint-cups=disabled -Dvulkan=disabled -Dcloudproviders=disabled -Dsysprof=disabled -Dtracker=disabled -Dcolord=disabled -Dintrospection=disabled -Ddemos=false -Dbuild-examples=false -Dbuild-tests=false -Dinstall-tests=false -Dgtk_doc=false -Dman-pages=false
ninja -C gtk-4.8.3/build install
```

Then build gtklab against it. The script points `pkg-config` and the dynamic
linker at the prefix, builds into `build-floor`, and runs the tests:

```{.bash include="ch17-shipping/scripts/floor-build.sh"}
```

```
scripts/floor-build.sh $HOME/floor/gtk48
```

### Install rules

`meson install` needs to know what to install. The executable gets
`install: true`, and a desktop entry goes into `share/applications`:

```{.meson include="ch17-shipping/meson.build" from="executable('gtklab'," to="install_dir: get_option('datadir') / 'applications')"}
```

```{.ini include="ch17-shipping/data/io.github.gtklab.desktop"}
```

The file's name is the application id from `main.cpp`,
`io.github.gtklab`. Desktops use that match, and `StartupWMClass` on X11, to
tie a running window to its launcher, so the dock shows one icon rather than
two. Check the entry with `desktop-file-validate`, then:

```
meson setup build-release --buildtype=release --prefix=$HOME/.local
meson install -C build-release
```

## Done when

On GTK 4.8.3, gtklab builds with no warnings, the core tests pass, and the
program runs: tabs, colours, the context menu, logging, zoom, and a 5 MB
`cat` that keeps repainting.

![gtklab running against GTK 4.8.3, built from the prefix.](images/ch17-floor.png){width=80%}

One caveat, measured on the machine the book was built on. GTK 4.8.3 built
against that machine's GLib 2.80 logs
`Attempting to freeze the notification queue for object GtkImage` criticals
from inside GTK whenever an image is finalized, the first from
`gtk_message_dialog_new`. That is GTK 4.8 and GLib 2.80 disagreeing, not
gtklab; Debian 12 pairs GTK 4.8 with GLib 2.74 and does not log them. On a
mixed build like this one, run floor checks without `fatal-criticals`, and
read any critical that does appear for a frame of your own code.

::: {.box title="Debugger drill"}
Debug the floor build in VS Code: copy the **gtklab** launch configuration,
point `program` at `build-floor/gtklab`, and add `LD_LIBRARY_PATH` to its
`env`, with the value the script printed. Set a breakpoint in `start_tab` and
open a tab. In the Debug Console, evaluate `gtk_get_minor_version()`: it
returns 8. A wrong `LD_LIBRARY_PATH` builds against 4.8 and runs against your
system GTK, and this is the quickest way to know which one you are testing.
:::

::: {.box title="Break it"}
Add a call to `gtk_file_dialog_new()` anywhere in `app_window.cpp`. With the
version macros in place, the normal build fails:

```
error: 'GtkFileDialog* gtk_file_dialog_new()' is deprecated: Not available before 4.10
```

Now remove `-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_8` from
`gtk_floor_args` and build again. The normal build succeeds; the floor build
fails, because the function does not exist in GTK 4.8:

```
error: 'gtk_file_dialog_new' was not declared in this scope; did you mean 'gtk_file_filter_new'?
```
 The macros
move that failure from the floor machine to your desk. Put both back.
:::

## Common mistakes

**Testing only on your own machine.** It runs the newest GTK you have, which
is the one your users are least likely to have.

**Trusting the macros alone.** They cover GTK and GLib functions, nothing
else, and say nothing about behaviour.

**A desktop file named after the program, not the application id.** The
dock shows a second, generic icon for the running window.

**Shipping a debug build.** `--buildtype=release` for anything you hand to
someone.

::: {.box title="Read in tmssh"}
`scripts/build.sh`, which configures and builds each flavour; and
`scripts/appimage.sh`, which bundles tmssh with its libraries into one file
that runs on most distributions. Then `release_appimage_theme_pin` in
`src/theme_apply.cpp`, called from `main`: the GTK plugin of the AppImage
tool exports `GTK_THEME=Adwaita:<variant>` at start-up, which locks the
window to one variant and breaks the light and dark switch from Chapter 7.
tmssh unsets it and pins Adwaita itself.
:::

::: {.box title="Check yourself"}
Name three things the version macros cannot catch. What does
`floor-build.sh` change so that the build finds GTK 4.8? Why must the desktop
file's name match the application id?
:::

# Capstone — A second tool {.unnumbered}

gtklab is done. The last exercise is to prove the book's ideas are yours and
not gtklab's, by building something that is not a terminal, from an empty
repository, with no reference solution.

**The tool.** A log viewer, `logpeek`:

- It opens a file and **follows** it as it grows, like `tail -f`, using a
  `GFileMonitor` and the read-budget pattern from Chapter 11, at idle
  priority.
- It shows every line in a `GtkListView` and stays smooth at a **million
  lines**, which only factory reuse (Chapter 5) makes possible.
- A search entry **filters** the lines, debounced (Chapter 7) so that typing
  does not re-filter on every key.
- The header bar holds a **sparkline** of lines per second over the last
  minute, a widget of your own with `measure` and `snapshot` (Chapter 12).
- Every command (open, follow on and off, clear the filter, jump to the end)
  is an **action** with a shortcut and a menu entry (Chapter 7).
- Line parsing and filtering live in a **core library with tests**
  (Chapter 4).

**Done when.**

1. It runs clean in an ASan build with `G_DEBUG=fatal-criticals` through
   opening a file, following it while another program appends to it,
   filtering, and quitting with follow still on.
2. It builds and runs on the floor (this chapter).
3. You can explain each of the four strands from *How to use this book*,
   lifetime, the main loop, model and widget, and input propagation, using
   your own code, without opening this book.

**Hints, if you need them.** The list's model can be a `GListStore` of line
objects; filtering it is a `GtkFilterListModel` with a `GtkCustomFilter`, and
`gtk_filter_changed` re-runs it. Measure your follow loop with `seq 1
1000000 >> file` while the window is open: if the sparkline stops moving,
check the priority of your watch first.
