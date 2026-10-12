# Chapter 1 — A window that runs

You start with the smallest GTK 4 program that behaves like an application:
a window that opens, can be resized, and reports its size when it closes. It
is twenty-five lines, and every one of them is doing something you will meet
again.

## The idea: the application owns the process

In GTK 4 a program is a `GtkApplication`. It parses the command line, runs
the main loop, and tells you when it is time to create windows by emitting
the `activate` signal. You never call a "main" function of GTK's yourself;
`g_application_run` runs the loop and returns only when the application
quits.

The application quits when it has no windows left. A window made with
`gtk_application_window_new(app)` is registered with the application when it
is created, so it keeps the application alive until it closes. That rule is
why the first Break it below is so short.

![What runs when: `main` hands control to the application, which emits `activate` once the main loop is running.](diagrams/app-lifecycle.svg)

The application id, `io.github.gtklab`, names the application on the
desktop and on D-Bus. By default GTK makes an application unique: a second
launch finds the first over D-Bus, asks it to activate, and exits. That is
the right behaviour for a document editor and the wrong one for a terminal
you may want to run twice with different settings, so gtklab passes
`G_APPLICATION_NON_UNIQUE` and every launch is its own process. tmssh does
the same.

A window that is asked to close emits `close-request` first. A handler can
return `TRUE` to keep the window open (to ask "save changes?", for example)
or `FALSE` to let it close. gtklab uses it to report the window's size; from
Checkpoint A it saves it.

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

::: {.box title="Check yourself"}
Who runs the main loop, and when does it stop? Why does
`G_APPLICATION_NON_UNIQUE` make a second launch a second process? What does
returning `TRUE` from `close-request` do?
:::
