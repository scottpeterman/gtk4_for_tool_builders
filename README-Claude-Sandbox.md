# Working on this repo from a Claude sandbox

Notes for a Claude session (or anyone) working in a fresh cloud container:
what to install, what is blocked, and how to run and check things headless.
They were collected while writing the book in an Ubuntu 24.04 container with
GTK 4.14.5, GLib 2.80, Meson 1.3.2 and pandoc 3.1.3.

## Git

- The sandbox can fetch this public repo but usually cannot push: the git
  proxy only injects credentials for repositories linked to the session. If a
  push returns 403, do not retry; hand back the files (or a `git bundle`) and
  let the owner push.
- The owner's clone ignores `build*`. That also matches `book/build.sh`, so add
  it with `git add -f book/build.sh` if it is ever re-created. Keep its
  executable bit: `git update-index --chmod=+x book/build.sh`.
- The fetched `subprojects/utf8proc/` directories in ch14-ch17 are build
  output; keep them out of commits (`.git/info/exclude` is enough locally).

## Packages

Build and run the chapters:

```
sudo apt-get install -y build-essential meson ninja-build pkg-config libgtk-4-dev librsvg2-common gdb
```

`librsvg2-common` matters: without it GTK cannot load SVG icons, and the
Chapter 15 icon and many theme icons render as "image-missing".

Run GTK programs headless and take screenshots:

```
sudo apt-get install -y xvfb openbox xcompmgr xdotool wmctrl imagemagick
```

Build the book:

```
sudo apt-get install -y pandoc texlive-xetex texlive-latex-extra texlive-fonts-recommended lmodern fonts-texgyre fonts-ibm-plex librsvg2-bin graphviz desktop-file-utils
```

## Network quirks

- Meson's WrapDB is blocked, which is why doctest is vendored as one header in
  each chapter's `tests/`. Do not switch it to a wrap.
- GitHub is reachable, so ch14-ch17 fetch utf8proc 2.11.3 through
  `subprojects/utf8proc.wrap` on first configure.
- There is no Docker daemon. The Chapter 17 Debian 12 one-liner cannot be run
  here; use the GTK 4.8 prefix build below instead.
- `rsync` is not installed; copy trees with a tar pipe.

## Verify everything

```
tools/verify-all.sh
```

Clean-builds every chapter and lab in debug, runs the unit tests and counts
warnings. Every line should read `warnings(own/all)=0/0` and `tests=pass` or
`tests=-`.

## Run a GTK program headless

`tools/shot.sh` starts Xvfb, openbox and a compositor, runs a program, finds
its window by title, optionally runs an action script, screenshots the
window, closes it through the window manager and prints the program's
output:

```
WINNAME="gtklab" tools/shot.sh ch08-tabs/build-debug/gtklab /tmp/ch08.png tools/act-ch08.sh
```

- The window title must match `WINNAME` (it is a regex anchored at the
  start). The labs use their own titles, for example "Tree Model Debugging
  Lab".
- An action script gets `$WID` and can call `xdotool ... --window $WID x y
  click 1` (coordinates are relative to the window) and `snap <file.png>`.
- The program's stdout and stderr go to the output name with `.log`.
- The final screenshot overwrites any `snap` with the same file name.
- To pass environment variables (ASan, fatal criticals, a scratch config),
  give shot.sh a small wrapper script instead of the binary.

## gtklab needs a scratch config

gtklab reads `$XDG_CONFIG_HOME/gtklab/sessions.ini`. For runs, point
`XDG_CONFIG_HOME` and `XDG_DATA_HOME` at a scratch directory and copy
`tools/demo_sessions.ini` there as `gtklab/sessions.ini`. Delete
`settings.ini` between runs, or the restored window size and sidebar state
make click coordinates miss. The demo ssh sessions point at hosts that do
not exist, so their tabs close themselves after a moment; use the "localhost"
session for anything that must stay open.

## Sanitizers and criticals

- ASan builds: `meson setup build-asan --buildtype=debug -Db_sanitize=address`.
  Run with `ASAN_OPTIONS=detect_leaks=0`; GTK, fontconfig and Mesa keep
  allocations until exit and bury real reports.
- `G_DEBUG=fatal-criticals` turns a CRITICAL into SIGTRAP (exit 133 outside a
  debugger). Use both together, as the "(ASan)" launch configuration does.
- For a stack at a critical: `gdb -q -batch -ex run -ex bt ./build-debug/gtklab`
  inside the wrapper script.

## The GTK 4.8 floor

A GTK 4.8.3 build lives in a prefix; build it once (about ten minutes):

```
git clone --depth 1 --branch 4.8.3 https://github.com/GNOME/gtk.git gtk-4.8.3
meson setup gtk-4.8.3/build gtk-4.8.3 --prefix=$HOME/floor/gtk48 --buildtype=release -Dx11-backend=true -Dwayland-backend=false -Dbroadway-backend=false -Dmedia-gstreamer=disabled -Dmedia-ffmpeg=disabled -Dprint-cups=disabled -Dvulkan=disabled -Dcloudproviders=disabled -Dsysprof=disabled -Dtracker=disabled -Dcolord=disabled -Dintrospection=disabled -Ddemos=false -Dbuild-examples=false -Dbuild-tests=false -Dinstall-tests=false -Dgtk_doc=false -Dman-pages=false
ninja -C gtk-4.8.3/build install
```

Then build any chapter against it from that chapter's directory (the script
builds the current directory into `build-floor`):

```
../ch17-shipping/scripts/floor-build.sh $HOME/floor/gtk48
```

GTK 4.8.3 against
this container's GLib 2.80 logs `Attempting to freeze the notification queue
for object GtkImage` criticals from inside GTK when images are finalized; that
is the version mismatch, not the code, so do not use fatal-criticals on floor
runs. The 4.8.3 source tree is also the quickest way to answer "what does GTK
actually do here".

## The book

```
cd book
bash build.sh all
```

- `./check-chapter.sh chapters/<file>.md` checks one chapter (includes resolve,
  images exist) without touching `out/`, so it is safe to run while someone
  else builds.
- Code blocks are filled from the chapter directories at build time
  (`filters/include-code.lua`). A missing file or anchor fails the build, so
  renaming a function means updating the chapter that includes it.
- Diagrams: edit `diagrams/src/*.dot`, then run `diagrams/render.sh`. It
  strips Graphviz's DOCTYPE line, which some hosts (claude.ai artifacts)
  refuse.
- Appendix F is generated from `answers/chNN.md` by build.sh; edit those, not
  `chapters/510-review-answers.md`.
- Line numbers quoted in the book (Chapter 16's stack, for example) come from
  the code; keep edits to `ch16-lifetime/src/app_window.cpp` above
  `on_page_removed` line-count neutral, or re-run the Break it and update the
  text.

## House rules for the text

- Shell commands one per line, never a backslash continuation.
- Never state output or behaviour that was not observed; run it first.