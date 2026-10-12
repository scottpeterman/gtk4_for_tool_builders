# Chapter 10 — Popovers and dialogs

The right-click that Chapter 9 claimed now opens a menu, the session tree
gets one too, closing a tab asks first, and sessions can be imported from a
file. All of it is built from two GTK 4 pieces, popovers and dialogs, and
both work differently from what GTK 3 code taught a generation of
developers.

::: {.box title="Recall"}
What does claiming a gesture sequence do (Chapter 9)? What happens after a
scroll handler returns `FALSE`? What owns a tab's `Tab` struct (Chapter 8)?
:::

## The idea: popovers are children, dialogs do not block

A **popover** is a widget like any other, with a parent. You create it,
parent it to a widget with `gtk_widget_set_parent`, tell it where to point
with `gtk_popover_set_pointing_to`, and pop it up. It is drawn above
everything else, but it lives in the widget tree under its parent, and it
dies with its parent. `GtkPopoverMenu` is a popover that renders a `GMenu`
model, so a context menu is the same data as Chapter 7's main menu: labels
naming actions.

Two rules follow from "a popover has a parent". First, parent it to a widget
that outlives anything the menu can do. The tab menu has a Close Tab item, so
it is parented to the notebook rather than to the tab it was opened from.
Second, a popover you create for one use must be unparented when it closes,
or it stays in the tree forever. And the moment it closes is not the moment
to unparent it, as the Break it shows.

A **dialog** is a window. In GTK 3, `gtk_dialog_run` ran a nested main loop
and returned the user's answer, so code could read top to bottom. That
function is gone. A GTK 4 dialog is shown and your function returns; the
answer arrives later in the `response` signal, and the code that acts on it
lives in that handler. Anything the handler needs must still be alive when
the user finally clicks, which may be minutes later.

## Build

### One helper for every popover menu

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::show_popover("}
```

Coordinates arrive in the clicked widget's space and must be converted to the
parent's. Use `gtk_widget_compute_point`, which exists in every GTK 4. Most
code you will find uses `gtk_widget_translate_coordinates`, which works but
is deprecated from GTK 4.12; the first version of this chapter used it too,
and it was changed when the deprecation was checked against the headers.

When the popover closes, it is unparented on the next idle, holding a
reference so it cannot vanish in between:

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::on_popover_closed("}
```

### The two menus

The tab menu makes the clicked tab current first, because the actions it
names (Copy, Close Tab) work on the current tab:

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::show_terminal_menu("}
```

The row menu needs to know which session was clicked. Row widgets are
recycled (Chapter 5), so the gesture cannot remember its session when it is
created. It keeps its `GtkListItem` instead and asks it at click time:

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::on_row_pressed("}
```

Its Open item names an action with a target, `win.open-session` with the
session's name, the same mechanism as Chapter 7's radio items:

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::show_row_menu("}
```

### A question that does not block

`confirm` shows a modal question and returns at once. What to do on "yes"
travels with the dialog in a small struct, along with a reference to the
object it acts on, which is released when the answer arrives:

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::confirm("}
```

Closing the current tab asks first. The callback holds the page, not the
`Tab` pointer, because the tab could be closed some other way while the
dialog is up, and checks that the page is still in a notebook before acting:

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::act_close_tab("}
```

### Importing a file

`GtkFileDialog` is the modern file picker, but it arrived in GTK 4.10, and
the version floor turns its use into a compile error. The 4.8 API is
`GtkFileChooserNative`, which also uses the platform's own picker where there
is one. It is a dialog too, so its answer arrives in a signal; the import
parses the file before replacing anything:

```{.cpp include="ch10-popovers/src/app_window.cpp" fn="void AppWindow::on_import_response("}
```

## Done when

Both menus open where you click; Close Tab asks first and Cancel leaves the
tab open; Import Sessions refuses a malformed file with a message.

![The tab menu, the row menu, and the confirm dialog.](images/ch10-popovers.png)

In the recorded run, importing a file containing `port=abc` showed "Not
imported: session 'bad': port is not a number" and left the tree as it was.

![Import Sessions: the file chooser, and the refusal of a malformed file.](images/ch10-import.png)

::: {.box title="Debugger drill"}
Set a breakpoint in `show_popover` and compare `x, y` with the converted
`px, py`. In the recorded run, a right-click at (258, 175) in the page was
(258, 212) in the notebook, which is the page offset by the tab strip; a
click at (91, 7) in a tree row was (104, 83) in the list view.
:::

::: {.box title="Break it"}
In `on_popover_closed`, call `gtk_widget_unparent` directly instead of on
idle, and choose Copy from the tab menu. Nothing happens, and nothing is
logged. The popover emits `closed` before it activates the chosen item's
action; unparenting it there destroys the menu that was about to activate
it. With the idle version, the action runs.
:::

## Common mistakes

**Parenting a context menu to the thing it can destroy.** In testing, GTK
4.14 survived a popover whose parent tab was closed from the popover's own
menu. Parenting to the notebook means not depending on that.

**Never unparenting.** Every popover you create and parent for one use stays
in the tree until you unparent it.

**Writing dialog code as if it blocked.** The line after showing the dialog
runs before the user has answered. Act in `response`.

**Holding a raw pointer across a dialog.** Hold a reference to an object, or
check that the thing still exists when the answer comes.

::: {.box title="Read in tmssh"}
`show_terminal_menu`, `show_row_menu`, `confirm`, `message` and
`act_import_file`.
:::

::: {.box title="Check yourself"}
Why is the tab menu parented to the notebook? Why is the popover unparented
on idle rather than in `closed`? Where does the code that acts on a dialog's
answer live?
:::

# Checkpoint C — Tabs and menus {.unnumbered}

**Spec.** Chapter 10, plus Next Tab and Previous Tab (Ctrl+PageDown and
Ctrl+PageUp, wrapping at the ends), and every shortcut reachable from the
main menu.

**Done when.** None of Chapter 9's four input sequences has changed after
adding the menus; and opening 50 tabs, choosing Close Other Tabs, then
closing the last with Ctrl+Shift+W prints exactly 50 `~Tab` lines. Both were
checked against the reference.

![Checkpoint C: every shortcut in the main menu.](images/ckC-menu.png){width=75%}

**Hints.** Next and previous are two more entries in the accelerator table
and the action table; `(current + 1) % n` and `(current + n - 1) % n` wrap.
GtkNotebook has its own Ctrl+PageDown binding, but it works only while focus
is inside the notebook; the window's capture-phase shortcut works from
anywhere, including the sidebar.

**Reference.** `ckC-tabs/`.
