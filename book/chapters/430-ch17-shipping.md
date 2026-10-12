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

::: {.box title="In this chapter"}
- Explain what the GTK and GLib version macros check, and what only a
  build on the floor can.
- Build and run gtklab against a real GTK 4.8, in Debian 12 or from a
  prefix.
- Choose a build type for what you are about to hand out.
- Install gtklab with a desktop entry whose name, and window, match the
  application id.
:::

## Concepts

### The floor is a machine, not a macro

The **floor** is the oldest platform you promise to support; for this book,
Debian 12, with GTK 4.8 and GLib 2.74. Since Chapter 0 every build has had
two pairs of macros that hold the code to it:

| Macro | Effect |
|---|---|
| `GDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_8` | anything marked newer than 4.8 in GTK's headers warns: `Not available before 4.10` |
| `GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_8` | deprecations from after 4.8 stay quiet, so 4.8's API is usable as is |
| `GLIB_VERSION_MAX_ALLOWED`, `GLIB_VERSION_MIN_REQUIRED` | the same for GLib, at 2.74 |
| `-Werror=deprecated-declarations` | turns those warnings into errors |

The mechanism is plain C. GTK's headers annotate each function with the
version that added it, `GDK_AVAILABLE_IN_4_10` for example. With
`MAX_ALLOWED` below 4.10, that annotation expands to a deprecation
attribute, and the compiler does the rest. `MIN_REQUIRED` works the other
way: `GtkFileChooserNative` is marked deprecated in 4.10, but with the
minimum at 4.8 that marking expands to nothing, which is why Chapter 10's
dialog compiles clean on a newer GTK under `-Werror`.

That is a compile-time check of names against annotations, on your own
machine, with your own GTK. It cannot catch:

- a change in **behaviour** between 4.8 and the GTK you develop on, such as
  a widget that measures differently or a signal emitted in a different
  order;
- a **library the macros do not cover**: gtkpyte calls Pango and cairo
  directly, and gtklab sets no version macros for either, so a newer Pango
  function compiles on your desk and fails on the floor;
- a **bug** in 4.8 that newer versions fixed.

![Two checks, two places. The macros run on every build; the floor runs before a release.](diagrams/ch17-floor.svg)

Only building and running on the floor itself catches these. Build there
before every release, and run the program, not just the tests: behaviour
is the one thing tests of a GTK-free core cannot see.

### Two ways to stand on the floor

| | Debian 12 (machine or container) | GTK 4.8 in a prefix |
|---|---|---|
| GTK | 4.8, as Debian ships it | 4.8.3, built by you |
| GLib, Pango, cairo | Debian 12's, the real pairing | your machine's, newer |
| Setup | one `docker run` | a GTK build, a few minutes |
| Trust | what a Debian 12 user runs | GTK's own code at 4.8, on newer neighbours |

The container is the truer test, because the floor is a whole platform, not
one library. The prefix is what you can do anywhere, and it is what this
chapter ran; the caveat in *Done when* below is exactly the cost of its
mixed pairing.

### Build types

Meson's `buildtype` picks optimisation and debug information together.
Measured on gtklab's own configuration:

| `--buildtype` | Compiler flags | gtklab binary |
|---|---|---|
| `debug` | `-O0 -g` | 2.5 MB |
| `debugoptimized` | `-O2 -g` | 3.9 MB |
| `release` | `-O3`, no debug info | 703 KB |

None of them defines `NDEBUG`: Meson's `b_ndebug` is `false` by default for
every build type, so `assert` stays active in a release build unless you ask
otherwise. The release build of gtklab compiled with no warnings, like the
debug one. Hand people `release`; keep `debugoptimized` for a build you
expect to debug on someone else's machine, since a stack from it still has
names and lines.

### The application id, the window and the desktop entry

`gtk_application_new("io.github.gtklab", ...)` names the application in a
form the desktop can use. The id is reverse DNS, by convention under a
domain you control; `io.github.<user>` is the usual choice for a project
hosted on GitHub. Several things are derived from it:

![What the application id reaches, and how a launcher ties a window to its entry.](diagrams/ch17-appid.svg)

- **The window's identity.** On Wayland, GTK gives the window the
  application id as its app id (when the program is on the session bus,
  as every desktop session provides). On X11 the window's `WM_CLASS` is
  the program name instead: measured, gtklab's window matched the class
  `gtklab` and not `io.github.gtklab`.
- **The resource base path**, `/io/github/gtklab`, which Chapter 15 used.
- **Single-instance negotiation**, on D-Bus. gtklab passes
  `G_APPLICATION_NON_UNIQUE`, which turns it off: every launch is its own
  process with its own window, as a terminal usually is.

A **desktop entry** is how a launcher, a menu and a dock know about the
program: its name, the command to run (`Exec`), an icon, categories. Its
file name should be the application id plus `.desktop`, because that is
what a Wayland desktop matches the running window's app id against.
`StartupWMClass=gtklab` covers X11, by naming the class the window really
has. Get either wrong and the dock shows the launcher's icon and a second,
generic icon for the running window.

`desktop-file-validate` checks the entry's syntax and keys; it passes for
gtklab's. It cannot check that the name matches the id: that is on you.

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

The macros move that failure from the floor machine to your desk. Put both back.
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

## Review

1. With the version macros at 4.8, a call to `gtk_file_dialog_new` fails to
   compile on a GTK 4.14 machine, yet the function exists there. What turns
   its presence in the header into an error?
2. Why does `GtkFileChooserNative`, deprecated in GTK 4.10, compile without
   a warning under `-Werror=deprecated-declarations`?
3. gtklab builds cleanly with the macros in place. Name a change you could
   make that would still compile on your desk and fail, or misbehave, on
   Debian 12.
4. The prefix build logged criticals that Debian 12 does not. Why, and what
   does that say about which floor to trust?
5. `floor-build.sh` succeeds, but the program you run afterwards reports
   GTK 4.14 from `gtk_get_minor_version()`. What went wrong?
6. Why is the desktop file named `io.github.gtklab.desktop` and not
   `gtklab.desktop`, and why does it also need `StartupWMClass=gtklab`?
7. Is `assert` active in gtklab's release build? Why?
8. When would you hand someone a `debugoptimized` build rather than a
   `release` one?

**Questions for deeper thought.** Where would you put your own floor for a
tool your team runs on a mix of laptops and jump hosts, and how would you
find out what those hosts actually have? Which of your tool's behaviours
would you check by hand on the floor before a release, given that the core
tests cannot see any of them? tmssh ships an AppImage that carries its
libraries with it; what does that change about the floor, and what does it
not?

## Exercises

1. **An icon of its own.** In `ch17-shipping`, add
   `data/icons/hicolor/scalable/apps/io.github.gtklab.svg`, install it to
   `share/icons/hicolor/scalable/apps` with `install_data` in
   `meson.build`, and set `Icon=io.github.gtklab` in the desktop file. Done
   when `desktop-file-validate` passes and, after
   `meson install -C build-release`, the launcher shows that icon.
2. **Validate in the tests.** In `meson.build`, find
   `desktop-file-validate` with `find_program(..., required: false)` and,
   when it is found, add it as a test on the desktop file. Done when
   `meson test` lists and passes it, and misspelling a key makes it fail.
3. **One version number.** Pass `meson.project_version()` to the compiler
   as `GTKLAB_VERSION`, and add a `win.about` action to
   `AppWindow::install_actions` with a menu entry in `build_menu` that
   calls `gtk_show_about_dialog`. Done when About shows `0.1.0`, and
   changing `version:` in `project()` changes it after a rebuild.
4. **The floor under ASan.** Give `scripts/floor-build.sh` an optional
   second argument, `asan`, that configures `build-floor` with
   `-Db_sanitize=address`. Done when the floor build runs gtklab through
   opening tabs, logging and quitting with no ASan report.

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
