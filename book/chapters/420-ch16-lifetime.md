# Chapter 16 — Lifetime, proven

Since Chapter 8, gtklab has quit with a warning you were told to keep. This
chapter explains it, fixes it the way tmssh does, and shows why the usual
memory tool could never have found it. Along the way it adds an
AddressSanitizer build, which finds the other kind of lifetime bug on the
first try.

::: {.box title="Recall"}
Which warning did you keep in Chapter 8, and when does it appear? What does
`g_signal_connect_object` do that `g_signal_connect` does not (Chapter 2)?
What must `~PtySession` do before anything else (Chapter 11)?
:::

## The idea: two kinds of lifetime bug

Every lifetime bug in a GTK program is code running on an object that should
no longer be used. They come in two kinds, and each needs its own tool.

**Use after free.** The object has been deleted and its memory returned to
the allocator; something still holds its address and reads through it.
Usually nothing visible happens, because the freed memory still holds what it
held a moment ago. Sometimes it crashes, somewhere else, later.
**AddressSanitizer** (ASan) makes this kind of bug deterministic. The
compiler instruments every memory access; freed memory is poisoned and kept
out of reuse for a while; the first access to it stops the program with three
stacks: the bad access, the free, and the allocation.

**Use during destruction.** The object is being destroyed but has not been
freed yet. Its destructor or `dispose` is running, and something it does
calls back into code that assumes the object is whole. The memory is still
allocated, so ASan has nothing to report. The members it reads may point at
objects that *have* been freed, but those belong to GTK and GLib, which are
not built with ASan. The evidence is a GTK `CRITICAL` from a failed type
check, and the tool is `G_DEBUG=fatal-criticals`, which turns that critical
into a stop in the debugger with the whole stack on screen.

The two are often found together, which is why the ASan launch configuration
sets both.

## Build

### An ASan build

A second build directory, beside `build-debug`:

```
meson setup build-asan --buildtype=debug -Db_sanitize=address
meson compile -C build-asan
```

The `build-asan` task in `tasks.json` does the same, and the **gtklab (ASan)**
launch configuration runs the result:

```{.json include="ch16-lifetime/.vscode/launch.json" from="\"name\": \"gtklab (ASan)\"," to="\"preLaunchTask\": \"build-asan\""}
```

`detect_leaks=0` matters. GTK, fontconfig and the Mesa drivers keep
allocations alive until exit by design, and at exit LeakSanitizer reports
every one of them, hundreds of lines that bury the one report you want. Leaks
are not lifetime bugs; turn their detector off here and look for leaks
separately if you need to.

Replay Chapter 11's Break it in this build: remove
`g_source_remove(read_watch_)` from `~PtySession`, run `yes` in a tab and
close the tab. ASan stops on the first try with `heap-use-after-free`: the
bad read is in `on_readable`, the allocation in `start_tab`, and the free in
the tab's destruction, all three in your own code. Put the line back.

### Who deletes the AppWindow

Since Chapter 8 the window has owned its `AppWindow` through object data,
deleted by the destroy function of `g_object_set_data_full`. Object data is
cleared when the window is finalized, at the very end of its life, and its
children have been torn down before that. The `AppWindow`'s handlers are
connected to those children, and they keep running while the children go.

The Chapter 8 warning is exactly that. Tearing down the window drops the
stack's pages, "empty" among them, and then the notebook removes its own
pages one by one, emitting `page-removed` each time. `on_page_removed`, which
holds `this`, asks the half-dismantled stack to show "empty".

The order in which GTK takes a window apart is not part of its API, and
tmssh's comments say so: do not depend on it. Its answer, which gtklab now
copies, has three parts. First, delete the `AppWindow` on the window's
`destroy` signal, emitted when the window is disposed, rather than waiting for
finalization, which comes only when the last reference to the window goes:

```{.cpp include="ch16-lifetime/src/app_window.cpp" from="// Delete this AppWindow on the window's \"destroy\"" to="this);"}
```

Second, hold a reference to the notebook, taken in `build_ui` once its
signals are connected. Whatever order the window's children go in, the
notebook, with its pages still in it, is alive when the destructor runs:

```{.cpp include="ch16-lifetime/src/app_window.cpp" from="// Our own reference: the notebook must outlive" to="g_object_ref(notebook_);"}
```

Third, the destructor disconnects every handler that carries `this` before
letting the notebook go, then removes every main-loop source that carries
`this`:

```{.cpp include="ch16-lifetime/src/app_window.cpp" fn="AppWindow::~AppWindow("}
```

`g_signal_handlers_disconnect_by_data` removes every handler on an instance
whose user data is the given pointer: one call for all the notebook's
handlers. After it, the pages can be removed in any order with nobody
listening.

One source was missing from that list until now. `switch-page` schedules an
idle to move the log check mark (Chapter 14), and that idle holds `this`.
Quit in the moment between a tab switch and the next idle, and it runs on a
deleted `AppWindow`. Its id is now kept:

```{.cpp include="ch16-lifetime/src/app_window.cpp" fn="void AppWindow::on_switch_page("}
```

The rule the destructor follows is the one that runs through the book:
anything that holds a raw pointer to you, a signal handler or a main-loop
source, must be gone before you are.

## Done when

Open several tabs, start a log in one, and quit, in the ASan build with
`G_DEBUG=fatal-criticals`. There is no ASan report, no `CRITICAL` and no
warning, and the log file ends with its footer. This was checked three runs
in a row; each run's debug log ended the same way, the two ssh tabs having
closed themselves earlier because their hosts did not exist:

```
gtklab-DEBUG: ~AppWindow
gtklab-DEBUG: ~Tab localhost
gtklab-DEBUG: ~SessionLog .../gtklab/logs/localhost-20261012-072632.log
```

`~AppWindow` runs first; the tabs go afterwards, with nobody listening; each
tab's log closes with it.

::: {.box title="Debugger drill"}
Set breakpoints in `~AppWindow`, `on_page_removed` and `~Tab`, open two tabs
and quit. `~AppWindow` stops first, called from the window's `destroy`
emission. Nothing in the destructor touches `stack_`, and the Break it below
shows why it must not. Continue: `on_page_removed` is never hit, because its
handler was disconnected; `~Tab` is hit once per tab, from
`g_object_unref(notebook_)`.
:::

::: {.box title="Break it"}
Keep the notebook reference but delete the
`g_signal_handlers_disconnect_by_data(notebook_, this)` line. Quit with a tab
open in the ASan build. ASan reports nothing, yet GLib prints:

```
GLib-GObject-CRITICAL **: invalid unclassed pointer in cast to 'GtkStack'
```

Run it under **stop on GTK criticals** and read the stack (trimmed):

```
#2  g_type_check_instance_cast
#3  AppWindow::on_page_removed at ../src/app_window.cpp:482
#4  ??? at libgtk-4.so.1
     ... g_signal_emit ...
#10 g_object_unref at libgobject-2.0.so.0
#11 AppWindow::~AppWindow at ../src/app_window.cpp:46
#12 operator() at ../src/app_window.cpp:24
```

The handler ran *inside* `~AppWindow`, from `g_object_unref(notebook_)`: the
notebook's last reference went, it removed its pages, and `page-removed`
called into an `AppWindow` that was being destroyed but not yet freed. Its
`stack_` pointed at a stack the window had already destroyed. That is the
Chapter 8 warning, now a critical because the order changed. ASan was silent
because the `AppWindow` itself was never freed memory; the stack was, but it
was GTK's. This is why the book keeps both tools.
:::

## Common mistakes

**Running with leak detection on.** The bug you want is somewhere in four
hundred lines of GTK's intentional exit-time allocations.

**Concluding "ASan is clean, so it is fine".** ASan sees only use after
free in instrumented code. Use during destruction, and anything inside GLib
or GTK, needs fatal criticals.

**Relying on teardown order.** GTK does not promise which child goes first.
Hold what your destructor needs, and disconnect before you let go.

**Removing sources but not handlers, or the reverse.** Both hold `this`.
List them in the destructor, one per member that holds a source id or an
instance with your handlers on it.

::: {.box title="Read in tmssh"}
`AppWindow::~AppWindow`; the `destroy` connection in the constructor; and
`g_object_ref(notebook_)` further down, with the comment explaining why
neither teardown order is safe without it. tmssh's destructor removes its
two timers, the reload and the save, the same way.
:::

::: {.box title="Check yourself"}
Name the two kinds of lifetime bug and the tool for each. Why was the
Chapter 8 bug invisible to ASan? Why does `build_ui` take a reference to the
notebook? What two kinds of thing does the destructor
remove before it lets go?
:::
