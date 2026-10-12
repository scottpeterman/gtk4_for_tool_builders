# Chapter 8 — Tabs that own things

Activating a session now opens a tab. A tab in a terminal application is not
just a widget; from Chapter 11 it owns a child process, a pseudo-terminal,
and later a log file. This chapter sets up the ownership so that all of that
is cleaned up in exactly one place, by closing the tab.

::: {.box title="Recall"}
What does `g_object_set_data_full` guarantee (Chapter 2)? Why does the
settings save use a timeout (Chapter 7)? What does the `win.` prefix mean?
:::

## The idea: the page owns the tab

A `GtkNotebook` page is a widget, and like any widget it is destroyed when it
is removed from its parent. So make the page own a `Tab` struct through
`g_object_set_data_full`, exactly as Chapter 2's button owned its `Counter`,
and closing a tab becomes "remove the page". Removing the page destroys it,
destroying it clears its data, and clearing its data deletes the `Tab`, whose
destructor releases everything the tab holds. There is no list of open tabs
to keep in sync and no cleanup function to remember to call.

The close button on each tab is connected with `g_signal_connect_object`,
with the page as its object. If the page is destroyed some other way first,
the connection goes with it, and the button can never call its handler with a
dead page.

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

::: {.box title="Check yourself"}
In what order do `page-removed` and `~Tab` run? Why is the close button
connected with `g_signal_connect_object`? Which object owns the `Tab`?
:::
