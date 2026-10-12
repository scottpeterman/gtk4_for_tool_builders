# Chapter 1 — A window that runs

You start with the smallest GTK 4 program that behaves like an application:
a window that opens, can be resized, and reports its size when it closes. It
is twenty-five lines, and every one of them is doing something you will meet
again.

::: {.box title="In this chapter"}
- Write the smallest correct GTK 4 program: an application, an `activate`
  handler and one window.
- Explain who runs the main loop, what keeps it running and what ends it.
- Choose between a unique application and one process per launch, and say
  what each does on a second launch.
- Intercept a window closing with `close-request`.
- Read any GTK call stack by recognising the shape every callback shares.
:::

## Concepts

### The application owns the process

In GTK 4 a program is a `GtkApplication`. It parses the command line, runs
the main loop, and tells you when it is time to create windows by emitting
the `activate` signal. You never call a "main" function of GTK's yourself;
`g_application_run` runs the loop and returns only when the application
quits. Your `main` is three lines of setup and a hand-over.

![What runs when: `main` hands control to the application, which emits `activate` once the main loop is running.](diagrams/app-lifecycle.svg)

An application emits three signals over its life, in a fixed order:

| Signal | When | What belongs there |
|---|---|---|
| `startup` | once, before anything else, in the process that will run the program | application-wide setup that must exist before any window |
| `activate` | when the program is launched with no files to open, after `startup` | create and present a window |
| `shutdown` | once, after the main loop ends | application-wide teardown |

Connecting a handler to each and closing the window prints them in that
order: `startup`, `activate`, the `close-request` line, `shutdown`, and then
`g_application_run` returns. gtklab only needs `activate`. The other two
matter once there are things that outlive a window.

`activate` is also the first moment GTK is ready. `main` runs before the
main loop and before the display connection exists, so nothing that draws
can be built there.

### What keeps the loop running

The application quits when it has no windows left. A window made with
`gtk_application_window_new(app)` is registered with the application when it
is created, so it keeps the application alive until it closes. Two windows
keep it alive until both have closed. A window that was never registered
keeps nothing alive, which is why the first Break it below is so short.

This rule is worth holding on to because it decides when your cleanup runs.
Everything after `g_application_run` in `main`, including the final
`g_object_unref(app)`, happens after the last window is gone.

### One process, or one per launch

The application id, `io.github.gtklab`, names the application on the
desktop and on D-Bus. By default GTK makes an application unique: a second
launch registers the same id, finds the first process holding it, asks it
to activate, and exits. Measured with the default flags under a D-Bus
session: the second launch printed nothing and returned 0, and the first
process received a second `activate`, so it opened a second window.

![What a second launch does. With default flags the first process does the work; with `G_APPLICATION_NON_UNIQUE` each launch is independent.](diagrams/ch01-instances.svg)

That is the right behaviour for a document editor and the wrong one for a
terminal you may want to run twice with different settings, so gtklab
passes `G_APPLICATION_NON_UNIQUE` and every launch is its own process.
tmssh does the same. The trade-off is real: a unique application can share
one session list and one settings file safely, because only one process
ever writes them. A non-unique one has to tolerate another copy saving the
same file; Chapter 4's atomic save at least guarantees that file is never
left half-written.

### Asking a window to close

A window that is asked to close, by the window manager's close button or by
`gtk_window_close`, emits `close-request` first. A handler can return `TRUE`
to keep the window open (to ask "save changes?", for example) or `FALSE` to
let it close. When every handler returns `FALSE`, GTK destroys the window,
and if it was the last one the application quits. gtklab uses the signal to
report the window's size; from Checkpoint A it saves it.

Returning a value from a signal handler is unusual in GTK. Most handlers
return `void`, because the signal is only a notification. `close-request`
returns a `gboolean` because it asks a question, and the answer changes what
GTK does next.

### Every callback has the same shape

After `activate`, nothing in your program runs on its own. Every line you
write runs because the main loop took an event from a source (input, a
timer, a file descriptor becoming readable, a frame to draw), turned it into
a signal, and the signal called your handler. When the handler returns,
control goes back to the loop.

![The shape of every GTK callback: your handler runs inside a signal emission, inside the main loop, inside `g_application_run`.](diagrams/ch01-callback-shape.svg)

Two consequences follow. A handler that takes a long time freezes the
window, because the loop that would redraw it is waiting for the handler to
return; Chapter 11 shows how to wait on slow things, a child process or a
file, without blocking it. And a
crash inside a handler shows this same stack, with your function at the top
and GTK's frames below, which is the first thing to recognise when you
read one.

### Coming from GTK 3

If you have written GTK 3, a handful of habits have to go:

| GTK 3 | GTK 4 |
|---|---|
| `gtk_init` then `gtk_main` | `GtkApplication` and `g_application_run`; `gtk_main` is gone |
| `gtk_widget_show_all` | nothing: widgets are visible by default |
| `delete-event` on a window | `close-request` |
| `gtk_container_add` | one call per container type (Chapter 3) |

## Build

`src/main.cpp`, complete:

```{.cpp include="ch01-window/src/main.cpp"}
```

Three details are easy to miss.

`gtk_window_get_default_size` returns the size the window has now, not the
size it was created with. GTK 4 updates a window's default size as the user
resizes it, which is what makes it the right value to save.

`gtk_window_present` shows the window and asks the window manager to raise
it. GTK 4 widgets are visible by default, so there is no
`gtk_widget_show_all`; a window is the one widget you still present.

The application is unreferenced after `g_application_run` returns. It was
created with one reference, which `main` owns.

## Done when

The window opens, you resize it and close it, and the terminal prints the
new size. In the recorded run the window was resized to 1100 by 700 and
closed, and the program printed `closing at 1100x700`.

![Chapter 1: an empty window, opened by `on_activate`.](images/ch01-window.png){width=70%}

::: {.box title="Debugger drill"}
Set a breakpoint in `on_activate` and press F5. When it stops, read the call
stack from the bottom up: `main`, then `g_application_run`, then the main
loop dispatching a source, then a signal emission, then your handler. That
shape, your code called from a signal emitted from the main loop, is the
shape of every GTK callback you will ever debug. Set a second breakpoint in
`on_close_request` and close the window to see the same shape again.
:::

::: {.box title="Break it"}
Replace `gtk_application_window_new(app)` with `gtk_window_new()` and run.
The program exits at once with status 0. The window was created, but it was
never added to the application, so as soon as `activate` returned the
application had no windows and quit. (`gtk_window_set_application` would add
it, which is all `gtk_application_window_new` does differently, plus a menu
bar that gtklab does not use.)
:::

## Common mistakes

**Writing a GTK 3 main.** `gtk_init` followed by `gtk_main` is GTK 3.
`gtk_main` no longer exists. Use `GtkApplication` and `g_application_run`.

**Building windows before `activate`.** `main` runs before the main loop and
before the display is open. Create windows in `activate`, never in `main`.

**Leaving the default flags on a tool you run twice.** Without
`G_APPLICATION_NON_UNIQUE` the second launch raises the first window and
exits, which looks like a bug when you wanted a second instance.

::: {.box title="Read in tmssh"}
`src/main.cpp`: `main` parses tmssh's own options before the application
runs, and `on_activate` creates the window. Then `on_close_request` in
`src/app_window.cpp`, which saves settings where gtklab prints.
:::

## Review

1. `main` creates the application and calls `g_application_run`, yet your
   window code lives in `on_activate`. Why can it not live in `main`?
2. Name everything that keeps gtklab's process running after `activate`
   returns. What is the one event that ends it?
3. A user launches gtklab twice. Describe what happens with
   `G_APPLICATION_NON_UNIQUE`, and what would happen without it.
4. You want a tool where a second launch opens a new window in the existing
   process. Which flag choice gives you that, and what does `on_activate`
   have to tolerate as a result?
5. Why does a `close-request` handler return a value when a `clicked`
   handler does not? What does GTK do if your handler returns `FALSE`?
6. A handler runs a five-second ping before returning. What does the user
   see during those five seconds, and why?
7. You call `g_print` after `g_application_run` returns. Is any window still
   open at that point? Explain from the rule that keeps the loop running.

**Questions for deeper thought.** For a network tool, is it better to have
one process holding every open session or one process per window? Think
about crash isolation, shared settings and memory. What would your tool
need to do differently if a second launch had to hand a host name to an
already-running instance?

## Exercises

1. **Startup and shutdown.** In `main` in `ch01-window/src/main.cpp`,
   connect handlers to the application's `startup` and `shutdown` signals
   that print their names. Done when one run prints `startup`, then the
   closing size, then `shutdown`.
2. **Confirm before closing.** Change `on_close_request` so that the first
   close is refused (return `TRUE` and print "close again to quit") and the
   second is allowed. Done when the window survives one click on the close
   button and closes on the second.
3. **Two windows.** Make `on_activate` create and present two windows, each
   with its own title. Done when closing either leaves the program running
   and closing the second ends it with both sizes printed.
4. **See uniqueness.** Change the flags in `main` to
   `G_APPLICATION_DEFAULT_FLAGS` and launch gtklab twice from two
   terminals. (The older spelling, `G_APPLICATION_FLAGS_NONE`, is deprecated
   in GLib 2.74, and the version floor rejects it.) Done when the second
   command returns at once and the first process opens a second window. Put
   `G_APPLICATION_NON_UNIQUE` back afterwards.
