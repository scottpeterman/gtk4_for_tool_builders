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

::: {.box title="In this chapter"}
- Tell the two kinds of lifetime bug apart, and pick the tool that finds
  each.
- Say what happens in `dispose` and in `finalize`, and which one your
  cleanup should hang on.
- Build gtklab with AddressSanitizer and read its three stacks.
- Turn GTK criticals into debugger stops with `G_DEBUG=fatal-criticals`.
- Audit any object that hands out `this`: every handler, source and
  reference it needs gone, or kept, before it goes.
:::

## Concepts

### Two kinds of lifetime bug

Every lifetime bug in a GTK program is code running on an object that should
no longer be used. They come in two kinds, and each needs its own tool.

**Use after free.** The object has been deleted and its memory returned to
the allocator; something still holds its address and reads through it.
Usually nothing visible happens, because the freed memory still holds what it
held a moment ago. Sometimes it crashes, somewhere else, later.
**AddressSanitizer** (ASan) makes this kind of bug deterministic.

**Use during destruction.** The object is being destroyed but has not been
freed yet. Its destructor or `dispose` is running, and something it does
calls back into code that assumes the object is whole. The memory is still
allocated, so ASan has nothing to report. The members it reads may point at
objects that *have* been freed, but those belong to GTK and GLib, which are
not built with ASan. The evidence is a GTK `CRITICAL` from a failed type
check, and the tool is `G_DEBUG=fatal-criticals`.

![A GObject's life, and where each kind of bug lives.](diagrams/ch16-stages.svg)

The two are often found together, which is why the ASan launch configuration
sets both.

### Dispose, then finalize

A GObject dies in two steps, and the gap between them is where the second
kind of bug lives.

| | `dispose` | `finalize` |
|---|---|---|
| Runs when | the last reference goes, or `g_object_run_dispose` forces it | after dispose, once the last reference has gone |
| Job | drop references to other objects, break cycles | free the object's own memory |
| Runs | possibly more than once | exactly once |
| What fires | a widget's `destroy` signal; then the object's signal handlers are dropped; then weak-reference notifies | destroy functions of `g_object_set_data_full` |
| The object afterwards | still valid memory, still its type | gone |

Measured with a label holding one extra reference: `g_object_run_dispose`
printed the `destroy` handler and the weak-reference notify, and the label
was still a `GtkLabel` afterwards; only the final `g_object_unref` ran the
object-data destroy function. A disposed object can be referenced and even
called into. It is just no longer whole.

That table is the whole argument of this chapter's fix. Since Chapter 8 the
`AppWindow` was deleted by an object-data destroy function, the last thing
that happens to the window. Its handlers on the window's children stayed
connected through everything before it.

### What a window's teardown promises

Very little. The window is disposed when it is destroyed; its `dispose`
releases its child, and the child's subtree goes with it as references
drop; at some point in that process the `destroy` signal is emitted. In
GTK 4.8's source, `GtkWindow` releases its child before it chains up to
`GtkWidget`'s dispose, which is what emits `destroy`. That is an
implementation detail, not API, and tmssh's comments say not to rely on
either order. What a `destroy` handler can assume is this: it runs when the
window is disposed, and some of the window's children may already be gone.

So the `AppWindow` cannot trust the widgets it built, with one exception: a
widget it holds its own reference to. A reference keeps the object alive,
undisposed, whatever its parent did. gtklab holds the notebook, because the
notebook is the one object the destructor must touch: its handlers carry
`this`, and its pages own the tabs.

![gtklab's teardown, in the order the code guarantees.](diagrams/ch16-teardown.svg){width=45%}

### What ASan does, in enough detail to trust it

ASan is a compiler pass plus a runtime library. The pass inserts a check
before every load and store in the code it compiles. The runtime keeps
**shadow memory**: one byte for every eight bytes of the process's memory,
saying how many of those eight are addressable. A check is a shift, an add
and a compare against the shadow byte.

![ASan's shadow check and quarantine.](diagrams/ch16-asan.svg)

The runtime also replaces `malloc` and `free` for the whole process,
including GTK's and GLib's calls. Each allocation gets **redzones**,
poisoned bytes on either side, so an overrun lands on poison. A free poisons
the whole block and puts it in **quarantine**, a first-in-first-out queue
(256 MB by default here) that keeps freed memory from being handed out
again for a while. Without quarantine, a freed `Tab` would soon be reused by
the next allocation, and a stale read would see valid-looking data.

What ASan cannot see follows from that design. It checks only accesses in
instrumented code: yours. A stale pointer passed into GTK and dereferenced
there goes unchecked. And it knows nothing about object lifetimes, only
memory: a disposed object, or an `AppWindow` halfway through its destructor,
is allocated, addressable and clean.

`detect_leaks=0` in the launch configuration turns off LeakSanitizer, which
otherwise reports every allocation still alive at exit; GTK and its drivers
keep many alive on purpose.

### What fatal-criticals does

GTK and GLib check their arguments. A cast macro such as `GTK_STACK(w)`
checks the instance's type; a `g_return_if_fail` checks a precondition.
A failure logs a `CRITICAL` and returns, and the program carries on with
the bug behind it. `G_DEBUG=fatal-criticals` makes criticals fatal: GLib
prints the message, then raises `SIGTRAP`. Under a debugger that is a stop,
with the stack on screen at the exact frame that made the bad call. Without
one the process dies; measured, the shell reported
`Trace/breakpoint trap` and exit status 133.

Warnings stay non-fatal under `fatal-criticals`. The Chapter 8 message is a
`Gtk-WARNING`, so stopping on it needs `G_DEBUG=fatal-warnings`, which makes
warnings and criticals both fatal.

### A checklist for any object that hands out `this`

`AppWindow` is not a GObject; GTK cannot see the raw pointers it gives
away. Every one must be accounted for:

| What holds `this` | Undo it in the destructor | In gtklab |
|---|---|---|
| a signal handler on an object that can outlive you | `g_signal_handlers_disconnect_by_data` | `notebook_`, `sessions_monitor_` |
| a main-loop source | keep its id; `g_source_remove` | `sync_idle_`, `save_source_`, `reload_source_` |
| an object you must touch while dying | hold a reference; release it last | `notebook_` |
| whoever deletes you | hang it on the start of teardown, not the end | the window's `destroy` |

When the receiver is itself a GObject, Chapter 2's
`g_signal_connect_object` does the first row for you. For a plain C++
object, the destructor is the list.

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
~Tab localhost
gtklab-DEBUG: ~SessionLog .../gtklab/logs/localhost-20261012-072632.log
gtklab-DEBUG: ~PtySession
```

`~AppWindow` runs first; the tabs go afterwards, with nobody listening; each
tab's log closes with it, then its session. (`~Tab` prints with `g_print`,
so it has no `gtklab-DEBUG` prefix; timestamps are trimmed.)

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

## Review

1. A tab's `PtySession` is deleted but its read watch is still installed,
   and output arrives. Which kind of lifetime bug is this, what does a
   normal build usually show, and what does the ASan build show?
2. Why could ASan never have reported the Chapter 8 warning? And in this
   chapter's Break it, where the stack really was freed, why was it still
   silent?
3. Since Chapter 8 the `AppWindow` was deleted by an object-data destroy
   function. Using the difference between `dispose` and `finalize`, explain
   why that was too late.
4. A `destroy` handler on the window wants to read the text of a label
   inside the window. Is that safe? What would make it safe?
5. What does the notebook reference taken in `build_ui` guarantee to
   `~AppWindow`, and why is the reference released only after the handlers
   are disconnected?
6. Why does ASan keep freed memory in quarantine instead of returning it to
   the allocator at once?
7. You run gtklab with `G_DEBUG=fatal-criticals` outside a debugger and it
   dies with `Trace/breakpoint trap`. What happened, and what is the next
   step?
8. Why did the idle scheduled from `switch-page` need its id stored, when
   it removes itself after one run?

**Questions for deeper thought.** `AppWindow` is a plain C++ object that
gives `this` to GTK in a dozen places. What would change if it were a
GObject subclass with its own `dispose`, and what would not? Your tool runs
a background thread that posts results back with `g_idle_add`; what does
the checklist in this chapter say about the window closing while a result
is in flight? Is a test suite that runs every build under ASan and
`fatal-criticals` worth its cost for a tool of a few thousand lines?

## Exercises

1. **The audit.** In `ch16-lifetime/src/app_window.cpp`, find every call
   that gives `this` (or `self`) to GTK or GLib: `g_signal_connect`,
   `g_idle_add`, `g_timeout_add`, action entries, dialog responses. For
   each, write down the line in `~AppWindow` that undoes it, or why none is
   needed. Done when every row has one or the other, and any row that has
   neither has been fixed.
2. **Teardown in the debug log.** `~Tab` in `src/tab.h` prints with
   `g_print`, unlike the other destructors. Change it to `g_debug`. Done
   when, with `G_MESSAGES_DEBUG=gtklab`, quitting with a logging tab prints
   `~AppWindow`, then for each tab `~Tab` and `~PtySession`, with
   `~SessionLog` between them for the logging tab, all with the
   `gtklab-DEBUG` prefix, and nothing at all without the variable.
3. **Catch a use after free on purpose.** In `AppWindow::close_tab`, keep
   the `Tab *` in a static and schedule an idle that prints
   `tab->session.name`. Done when the ASan build reports
   `heap-use-after-free` and you can match each of its three stacks to a
   function of your own: the idle, `close_tab`'s page removal, and
   `add_tab`. Then remove it.
4. **The original warning, stopped.** Run the `ch08-tabs` build under the
   debugger with `G_DEBUG=fatal-warnings`, open a tab and quit. Done when
   the debugger stops on `Child name 'empty' not found` and you can point
   to `on_page_removed` in the stack and the GTK frames below it that were
   tearing the window down.
