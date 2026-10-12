# Chapter 8 — Tabs that own things

Activating a session now opens a tab. A tab in a terminal application is not
just a widget; from Chapter 11 it owns a child process, a pseudo-terminal,
and later a log file. This chapter sets up the ownership so that all of that
is cleaned up in exactly one place, by closing the tab.

::: {.box title="Recall"}
What does `g_object_set_data_full` guarantee (Chapter 2)? Why does the
settings save use a timeout (Chapter 7)? What does the `win.` prefix mean?
:::

::: {.box title="In this chapter"}
- Separate application, window and tab scope, and say which object owns each.
- Make a notebook page own its `Tab`, so the notebook is the only list of tabs.
- Resolve the current tab when a command runs, instead of keeping a pointer
  to it.
- Trace a tab close from any path to `~Tab`, and say what is still alive at
  each step.
:::

## Concepts

### A tab is a workspace

A tab looks like a label over a panel, but in a tool it stands for
something running: from Chapter 11 a child process and its pseudo-terminal,
later a log file. Two rules follow. Switching tabs changes which workspace
you see, never which ones exist: an inactive tab keeps its process, its
scrollback and its zoom. And closing a tab must release everything that tab
holds, every time, by whatever route it was closed.

### Three scopes

Chapter 7 put commands on the window (`win.`) or the application (`app.`).
Tabs add a third scope that GTK has no prefix for.

| Scope | In gtklab | Lives until | Commands reach it through |
| --- | --- | --- | --- |
| Application | `GtkApplication`, which holds its windows | the process exits | `app.` actions |
| Window | `AppWindow`: settings, the session tree, the notebook, the debounced save | the window is destroyed | `win.` actions |
| Tab | `Tab`: a copy of its `Session`, pointers to its widgets; later its zoom and pty | its page is finalized | the notebook's current page, looked up when the command runs |

Keep state at the narrowest scope that needs it. A tab's zoom level belongs
to the tab (Chapter 9), not to the window, or switching tabs would carry one
tab's zoom into another. The settings belong to the window: `AppWindow`
loads them when it is built and saves them when the window closes.

Each `Tab` holds its own copy of the `Session` it was opened from. Reloading
the sessions file replaces the window's folders and rebuilds the tree's
items, and a tab that pointed into either would be left pointing at freed
memory. A copy is a
snapshot: the tab keeps the host and user it connected with, whatever the
file says now.

### The page owns the tab

There are two ways to keep a C++ object per tab. One is a registry: a
`std::vector<std::unique_ptr<Tab>>` in the window, next to the notebook,
with each entry remembering its page. The other is the one this book uses
and tmssh uses: no registry. A `GtkNotebook` page is a widget, and like any
widget it is destroyed when it is removed from its parent. So make the page
own its `Tab` through `g_object_set_data_full`, exactly as Chapter 2's button
owned its `Counter`.

![What owns what in a tab. Solid arrows own; the `Tab` hangs off the page as object data. Its widget pointers are borrowed, and the close button's connection is broken if the page goes first.](diagrams/ch08-page-owns-tab.svg)

| Question | Registry beside the notebook | Page owns the `Tab` |
| --- | --- | --- |
| How many lists of tabs? | Two, kept in sync by hand | One: the notebook |
| Tab dragged to a new position | The registry's order is now wrong | Nothing to do |
| Find the `Tab` for a page | Search the registry | `g_object_get_data(page, "tab")` |
| Who deletes the `Tab`? | Every close path must erase its entry | The page, when it is finalized |
| Window destroyed with tabs open | Depends on the order the window's members and widgets are torn down | Each page is finalized, each `~Tab` runs |

The registry works in a small program. It stops working the first time a
close path forgets to erase its entry, or a reorder leaves the indexes
wrong. With the page as owner there is nothing to forget: closing a tab
becomes "remove the page", and removing the page destroys it, which clears
its data, which deletes the `Tab`.

The close button on each tab is connected with `g_signal_connect_object`,
with the page as its object. If the page is destroyed some other way first,
the connection goes with it, and the button can never call its handler with
a dead page.

The `Tab`'s widget pointers are borrowed. The notebook owns the page and the
page owns the view; the `Tab` only remembers where they are. That has a
consequence for its destructor. A program that put a weak reference on the
view and logged the page's teardown printed, in this order:

```
view finalized
page destroy (dispose)
~Tab
```

By the time `~Tab` runs, the view is gone. A tab's destructor releases what
GTK does not know about (the pty in Chapter 11, a log file) and never
touches its widgets.

### Commands find the tab when they run

A command such as Copy or Close Tab acts on "the current tab". Do not keep a
`Tab *current_` that you update on every switch; it goes stale the moment a
tab closes or a drag reorders the pages. Ask the notebook at the moment the
command runs: current page number, then that page, then its `Tab` from the
object data. Chapter 10 writes exactly that as `current_tab()`.

There is one trap in the notebook's own signal. Inside a `switch-page`
handler, `gtk_notebook_get_current_page` does not yet return the new page.
Measured on a three-page notebook, switching to page 2 reported 0 (the old
page) inside the handler, and switching away from a page being removed
reported -1. Use the page the signal passes you.

### Closing is a lifecycle transition

A tab closes in more ways than one: its close button, the Close Tab and
Close Other Tabs commands (Chapter 10), its process exiting (Chapter 11), and
the window closing with tabs open. All of them end in the same place.

![Every close path ends in the same three steps. Cleanup in `~Tab` runs whichever path was taken.](diagrams/ch08-close-paths.svg)

| Stage | In gtklab | Where |
| --- | --- | --- |
| Ask | Close Tab asks first (Chapter 10) | the action, before anything is removed |
| Detach | `gtk_notebook_remove_page` | every close path |
| Update the window | Show "No tabs open" when the last tab goes | `page-removed` |
| Release | Close the pty, flush the log (Chapter 11 on) | `~Tab` |

Closing the current tab of three, the recorded order was: `switch-page` to
its neighbour, then `page-removed` with the page still alive, then `~Tab`.
That last step happens when the page's last reference goes, not when it
leaves the notebook. A test that took an extra reference on a page before
removing it saw `page-removed`, then a page with no parent and its `Tab`
still attached, and `~Tab` only when the extra reference was dropped.
Chapter 10's confirm dialog depends on exactly that.

Inside `~Tab`, the order matters too. C++ destroys members in reverse order
of declaration, and tmssh uses that: its `Tab` declares the log before the
pty, so the pty is closed first and the log's footer is the last thing
written.

## Build

`src/tab.h`:

```{.cpp include="ch08-tabs/src/tab.h"}
```

`add_tab` builds the page, attaches the `Tab`, and builds the tab label with
its close button. The text view is a placeholder until Part III:

```{.cpp include="ch08-tabs/src/app_window.cpp" fn="Tab *AppWindow::add_tab("}
```

The close handler gets the page and finds its notebook. A page's parent is
the notebook's internal stack, not the notebook itself, so it asks for the
nearest ancestor of the right type:

```{.cpp include="ch08-tabs/src/app_window.cpp" fn="void AppWindow::on_tab_close_clicked("}
```

The window's stack shows the notebook when a page is added, and the "No tabs
open" hint when the last one goes:

```{.cpp include="ch08-tabs/src/app_window.cpp" fn="void AppWindow::on_page_added("}
```

```{.cpp include="ch08-tabs/src/app_window.cpp" fn="void AppWindow::on_page_removed("}
```

Activating a session in the tree now calls `add_tab` instead of printing.

## Done when

Sessions open as tabs; tabs can be reordered by dragging; the close button
removes a tab; and `~Tab` prints once for every tab, whether it was closed or
still open at quit.

![Three tabs opened from the tree.](images/ch08-tabs.png){width=80%}

::: {.box title="Debugger drill"}
Set breakpoints in `~Tab` and in `on_page_removed`. Close a tab with its
button. The order is: `page-removed` first, while the page still exists, and
then `~Tab` as the page is destroyed.
:::

::: {.box title="Break it (keep this one)"}
You do not have to write this bug; Chapter 8 as written already has it. Open
three tabs and quit. The log shows:

```
Gtk-WARNING: Child name 'empty' not found in GtkStack
```

When the window is destroyed, its widgets are torn down. By the time the
notebook removes its pages, the stack's "empty" page has already gone, and
your `page-removed` handler, which holds `this`, runs against a window that
is half dismantled. Here it only produces a warning. Leave it in. Chapter 16
shows what it really is, why the usual memory tool cannot see it, and fixes
it the way tmssh does.
:::

## Common mistakes

**Keeping a list of tabs beside the notebook.** The notebook already is the
list. A second one has to be kept in sync on every open, close, reorder and
drag, and will not be.

**Using `gtk_widget_get_parent` to find the notebook.** It returns an
internal stack.

**Cleaning up in the close button's handler.** Tabs also close when their
process exits, when the window closes, and through Close Other Tabs.
Cleanup belongs in the destructor that every path reaches.

::: {.box title="Read in tmssh"}
`add_tab`, the deleter lambda passed to `g_object_set_data_full`, and the
`page-removed` handler in `build_ui`.
:::

## Review

1. Opening a session gives a tab a copy of its `Session`, not a pointer into
   the tree's items. What goes wrong with the pointer when the sessions
   file is reloaded?
2. A tab is dragged from position 3 to position 1. What does the code have
   to update, and why?
3. Why does Close Tab look up the current tab when it runs instead of
   reading a `current_` member kept up to date by `switch-page`?
4. Inside a `switch-page` handler, you call
   `gtk_notebook_get_current_page`. What do you get, and what should you
   use instead?
5. List the close paths a tab will have by Chapter 11. Why does cleanup
   belong in `~Tab` rather than in any of them?
6. Something holds an extra reference on a page while the tab is closed.
   Which of `page-removed` and `~Tab` runs at `remove_page`, and when does
   the other one run?
7. Why must `~Tab` not touch `tab->view`?
8. The close button is connected with `g_signal_connect_object` and the
   page as the object. What could happen with a plain `g_signal_connect`
   and the page as user data?

**Questions for deeper thought.** A future version detaches a tab into a
new window. With the page owning its `Tab`, what has to move, and what
would a window-owned registry have made harder? Which state in a tool you
know belongs at tab scope, but is kept at window scope today? If closing a
tab had to wait for a remote end to say goodbye, where would that waiting
state live, and what would the window show meanwhile?

## Exercises

1. **Tab count in the title.** In `ch08-tabs/src/app_window.cpp`, make
   `on_page_added` and `on_page_removed` set the window title to
   "gtklab (3 tabs)", or plain "gtklab" with none. Done when the title is
   right after opening, closing and dragging tabs.
2. **Follow the current tab.** In `build_ui`, connect `switch-page` and set
   the title to the session name of the page the signal passes. Done when
   the title follows clicks on tabs and is right after closing the current
   tab while others remain; then try `gtk_notebook_get_current_page` in the handler and see
   which title you get.
3. **Focus instead of duplicating.** In `activate_session`, walk the
   notebook's pages, read each page's `Tab` with `g_object_get_data`, and if
   one has the same session name, make it current instead of calling
   `add_tab`. Done when double-clicking an open session switches to its tab,
   and `~Tab` still prints once per tab at quit.
4. **Watch the order.** In `add_tab`, add a `g_object_weak_ref` on
   `tab->view` that prints "view gone" with the session name. Done when you
   can say, from the log, whether the view or the `Tab` goes first on a
   close and at quit, and why that rules out touching widgets in `~Tab`.

## Isolated lab

`labs/lab08-tabs` is the chapter's ownership design with everything else
taken away: no session tree, no actions, no settings. Each tab is a
`Session` with a counter, a page that owns it, and three buttons that act on
the current tab. Small enough to hold in your head, so you can break it on
purpose and see exactly what changes.

The `Session` prints when it is created and destroyed:

```{.cpp include="labs/lab08-tabs/src/main.cpp" fn="struct Session {" nocomment="1"}
```

The page owns it, and commands resolve it from the notebook when they run:

```{.cpp include="labs/lab08-tabs/src/main.cpp" fn="static Session *active_session("}
```

```{.cpp include="labs/lab08-tabs/src/main.cpp" fn="static void add_tab("}
```

```{.cpp include="labs/lab08-tabs/src/main.cpp" fn="static void close_active("}
```

The notebook's handlers carry the status label, connected with
`g_signal_connect_object` so that teardown order at quit cannot matter:

```{.cpp include="labs/lab08-tabs/src/main.cpp" fn="static void on_switch("}
```

```{.cpp include="labs/lab08-tabs/src/main.cpp" fn="static void on_page_removed(" nocomment="1"}
```

Build and run it:

```
cd labs/lab08-tabs
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/lab
```

Or open the folder in VS Code and run the "lab" configuration.

| Stop at | What you see |
| --- | --- |
| `add_tab` | A new `Session` and page; nothing holds the `Session` except the page's data |
| `active_session` | The current page number, then the `Session` from that page's data |
| `on_switch` | `page` is the new page; `gtk_notebook_get_current_page(nb)` is not yet its number (it was -1 when the current tab was being closed) |
| `close_active` | The index being removed |
| `on_page_removed` | The page is out of the notebook, but its `Session` is not yet destroyed |
| `Session::~Session` | Called from `g_datalist_clear` inside `g_object_unref`: the page's last reference just went |

Experiments:

1. Increment Session 2 three times, switch to Session 1 and increment once.
   In the recorded run Session 1 showed count 1 and Session 2 kept 3: the
   inactive tab was not touched. Quit printed `DESTROY session 1, count=1`
   and `DESTROY session 2, count=3`.
2. With breakpoints in `on_switch`, `on_page_removed` and
   `Session::~Session`, close the current tab of two. The recorded order was
   `on_switch` (to the remaining tab), then `on_page_removed`, then
   `~Session`.
3. Replace `g_object_set_data_full` in `add_tab` with `g_object_set_data`
   and no destroy function. Predict what quit prints, then run it.
4. In `close_active`, call `g_object_ref` on the page before removing it and
   never unref it. Predict which of `on_page_removed` and the `DESTROY` line
   still appears, then check.

![The lab after incrementing Session 1 once: the status line shows the current tab's count and the total.](images/lab08-tabs.png){width=70%}

