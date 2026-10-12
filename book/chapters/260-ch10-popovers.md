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

::: {.box title="In this chapter"}
- Show a context menu where the user clicked, parented to a widget that
  outlives everything the menu can do, and clean it up afterwards.
- Convert a point from the widget that was clicked to the popover's parent.
- Ask a question with a dialog that does not block, and act on the answer
  safely when it arrives.
- Follow one right-click through gesture, menu, action, dialog and state
  change, and log it at each boundary.
:::

## Concepts

### Three ways to ask

GTK offers several surfaces for asking the user something, and they are not
interchangeable.

| You need | 4.8 API used here | Its answer arrives in | From GTK 4.10 |
| --- | --- | --- | --- |
| A context menu | `GtkPopoverMenu` over a `GMenu` | the chosen item's action | the same |
| A yes/no question | `GtkMessageDialog` | the `response` signal | `GtkAlertDialog` |
| A file to open | `GtkFileChooserNative` | the `response` signal | `GtkFileDialog` |

The 4.10 headers mark `gtk_message_dialog_new`, `gtk_dialog_add_button` and
`gtk_file_chooser_native_new` deprecated, and `GtkAlertDialog` and
`GtkFileDialog` arrive in 4.10. The build's floor flags make 4.8 the
version that counts, so the old calls compile cleanly and the new ones do
not compile at all. Raise the floor to 4.10 and the deprecations turn into
errors under `-Werror=deprecated-declarations`: moving the floor is a port,
not a version bump.

### Popovers are children

A **popover** is a widget like any other, with a parent. You create it,
parent it to a widget with `gtk_widget_set_parent`, tell it where to point
with `gtk_popover_set_pointing_to`, and pop it up. It is drawn above
everything else, but it lives in the widget tree under its parent, and it
dies with its parent. `GtkPopoverMenu` is a popover that renders a `GMenu`
model, so a context menu is the same data as Chapter 7's main menu: labels
naming actions. Its items show the same accelerators, and grey out when
their action is disabled.

Two rules follow from "a popover has a parent". First, parent it to a widget
that outlives anything the menu can do. The tab menu has a Close Tab item,
so it is parented to the notebook rather than to the tab it was opened
from. Second, whoever calls `gtk_widget_set_parent` must call
`gtk_widget_unparent`; a parent does not drop a popover on its own. And the
moment the popover closes is not the moment to unparent it, as the Break it
shows: `closed` comes before the chosen item's action.

![A one-use context menu. The popover is unparented on idle, after the chosen action has run.](diagrams/ch10-popover-life.svg)

| | Long-lived popover | One per use |
| --- | --- | --- |
| Example | the lab's popover on its button; the menu inside Chapter 7's `GtkMenuButton` | gtklab's tab and row menus |
| Created | once, with its parent | on each right-click |
| Unparented | when its parent is destroyed (the menu button does this itself) | on idle after `closed` |
| Fits when | the anchor and the contents never change | the contents depend on what was clicked |

### Coordinates belong to a widget

A gesture reports `x, y` in the coordinates of the widget it is attached
to. The popover wants a rectangle in its parent's coordinates. Whenever an
API takes a point, say whose point it is. `gtk_widget_compute_point(from,
to, ...)` converts between any two widgets in the same window. In the
recorded run a right-click at (258, 175) in the page was (258, 212) in the
notebook: the page sits below the tab strip. Skip the conversion and the
menu opens 37 pixels above the pointer. The older
`gtk_widget_translate_coordinates` does the same job but is deprecated from
GTK 4.12.

### The menu names commands; it does not own them

A context menu is a command surface. Its items name actions (`win.copy`,
`win.close-tab`), and the actions resolve their target the way Chapter 8
said: they ask the notebook for the current tab when they run. So the tab
menu first makes the clicked tab current. That is a contract worth noticing:
"current" now means "the one you right-clicked".

The alternative is an action with a parameter that names the target. The
row menu does that, because a row is not "current" in any sense the actions
know about: its Open item names `win.open-session` with the session's name
as the target, the same mechanism as Chapter 7's radio items. Passing an
identifier costs a parameter and a lookup; relying on "current" costs a
side effect (the tab switches). For tabs the side effect is what a user
expects. For rows, which are recycled (Chapter 5), the gesture cannot even
remember its session; it keeps its `GtkListItem` and asks it at click time.

### Dialogs do not block

A **dialog** is a window. In GTK 3, `gtk_dialog_run` ran a nested main loop
and returned the user's answer, so code could read top to bottom. That
function is not in GTK 4's headers. A GTK 4 dialog is shown and your
function returns; the answer arrives later in the `response` signal, and the
code that acts on it lives in that handler. Modal does not mean blocking: a
modal dialog stops input to its parent window, but the main loop keeps
running, timers fire, and from Chapter 11 the other tabs keep receiving
output.

So the state when the question is asked may not be the state when the
answer comes. Anything the handler needs must still be alive, and anything
it assumes must be checked again. The close confirmation holds a reference
to the tab's page, not a `Tab *`. Chapter 8 measured what that buys: with a
reference held, a removed page keeps its `Tab` until the reference goes. The
handler then checks that the page is still in a notebook before closing it.

Treat every answer except the one you asked for as "no". In a recorded run,
pressing Escape on the confirm dialog delivered response -4
(`GTK_RESPONSE_DELETE_EVENT`), neither Accept nor Cancel. `confirm` acts
only on `GTK_RESPONSE_ACCEPT`, so Escape, Cancel and the window's close
button all leave the tab open.

An import is the same shape with more steps. Choosing a file is only the
first: the file is parsed by the Chapter 4 core before anything is
replaced, and a malformed file changes nothing.

| Step | Question | On failure |
| --- | --- | --- |
| Choose | Did the user accept? | Nothing happens |
| Validate | Does the file parse as sessions? | "Not imported" with the parser's message; the tree is untouched |
| Store | Can it be copied over the sessions file? | "Not imported" with the file error |
| Refresh | Reload and rebuild the tree | Expanded folders are kept |

### Tracing one click through the layers

By now a right-click crosses four layers: an input **controller**, a
**popover** that offers commands, an **action** that resolves its target,
and a **state change** that may wait for a dialog. They are separate
stories. Propagation decides which controller sees the click; it says
nothing about which tab Close Tab will close.

![One right-click, from gesture to `~Tab`. The dialog splits it in two: everything below the main loop happens in a later callback.](diagrams/ch10-trace.svg)

When something goes wrong (a menu in the wrong place, the wrong tab closed,
nothing happening at all) log at the boundaries where responsibility
changes, and include the target in every line. This is the recorded log of
right-clicking the second of two tabs, choosing Close Tab, and confirming:

```
page capture: button 3 claimed (menu at 258,175) [core2-den]
popover: from (258,175) to parent (258,212)
confirm response -3
page-removed, 1 left
~Tab core2-den
```

-3 is `GTK_RESPONSE_ACCEPT`. About two seconds passed between the second
line and the third, while the menu and the dialog were up. There is no line
for the action itself, because `act_close_tab` does not log. Set a
breakpoint there and one in `on_page_pressed` and compare the call stacks:
the action runs from the click on the menu item, a later event, and the
removal runs from the dialog's answer, later still. No single call stack
contains the whole story, which is why the log has to carry the target.

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

## Review

1. The tab menu is parented to the notebook. What could go wrong if it were
   parented to the page it was opened from, and which menu item causes it?
2. Why is the popover unparented on idle rather than in its `closed`
   handler? What would the user see otherwise?
3. A popover is parented to the list view, and the click arrived on a row.
   Whose coordinates does the gesture report, and what goes wrong without
   `gtk_widget_compute_point`?
4. The tab menu makes the clicked tab current; the row menu passes the
   session's name as an action target. Why the difference?
5. "Modal" and "blocking" are different things. What does a modal dialog
   stop, and what keeps running?
6. Why does the close confirmation hold a reference to the page rather
   than the `Tab *`, and what does it check when the answer comes?
7. The user presses Escape on the confirm dialog. What response arrives,
   and why does the code not need a case for it?
8. In the trace, why is there no call stack that contains both the
   right-click and the tab's removal?

**Questions for deeper thought.** A dialog that waits on the user is one
kind of asynchronous operation; a connection that waits on a remote host is
another. What does the "check again when the answer comes" rule look like
for the second? When would you rather put a question in a popover or an
inline banner than in a modal dialog? Your tool has to raise its version
floor from 4.8 to 4.10 one day: which parts of this chapter's code would
change, and how would you find them?

## Exercises

1. **Copy only with a selection.** In `ch10-popovers/src/app_window.cpp`,
   make `show_terminal_menu` disable `win.copy` (with
   `g_simple_action_set_enabled`) when the current tab's text buffer has no
   selection, and enable it otherwise. Done when Copy is greyed out in the
   menu until you select text. tmssh does this in its own
   `show_terminal_menu`.
2. **Close Tabs to the Right.** Add a `win.close-tabs-right` action beside
   `act_close_other_tabs` and a menu item in `show_terminal_menu`. Done when
   it closes every tab after the clicked one and prints one `~Tab` per
   closed tab.
3. **A target instead of "current".** Add a `win.close-session-tabs`
   action with an `"s"` parameter that walks the notebook's pages and
   closes every tab whose `Tab` has that session name, and add it to
   `show_row_menu` with the session's name as the target. Done when
   right-clicking a session in the tree and choosing the item closes all of
   its tabs, whichever tab is current, with one `~Tab` per tab.
4. **Ask before replacing.** In `on_import_response`, after the file has
   parsed, call `confirm` with "Replace the sessions file?" and do the copy
   in its accept callback. Done when Cancel leaves the tree as it was and
   accepting imports the file. `confirm` takes a reference to the object
   it carries, so decide which GObject the callback needs (the chosen
   `GFile` is one).

## Isolated lab

`labs/lab10-popover` puts the two mechanisms side by side with nothing
else around them: a long-lived popover on a button, and a modal question
whose answer arrives later. No menus or actions, so the parent and the
response are the only things to watch.

The popover is created once and parented to its button, and unparented when
the button is destroyed:

```{.cpp include="labs/lab10-popover/src/main.cpp" from="lab->button = gtk_button_new_with_label" to="lab->popover);"}
```

The dialog is shown and the function returns; the answer arrives in a
separate handler:

```{.cpp include="labs/lab10-popover/src/main.cpp" fn="static void on_confirm_clicked("}
```

```{.cpp include="labs/lab10-popover/src/main.cpp" fn="static void on_dialog_response("}
```

Build and run it:

```
cd labs/lab10-popover
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/lab
```

Or open the folder in VS Code and run the "lab" configuration.

| Stop at | What you see |
| --- | --- |
| `on_menu_clicked` | The popover already has the button as its parent; this only pops it up |
| `on_popover_closed` | The popover is closed but still parented, ready to open again |
| `on_confirm_clicked`, last line | The dialog is presented; continue, and the function returns before anyone has answered |
| `on_dialog_response` | `response` is the button's id; `lab` is the same struct the click had |
| the button's `destroy` handler | At quit: the popover is unparented while the button still exists |

Experiments:

1. Open the popover, then the dialog, and press Apply. The recorded run
   showed the popover under its button, a modal dialog, and then
   "Confirmed 1 time(s)".
2. Delete the `g_signal_connect(lab->button, "destroy", ...)` call and
   quit. With that code GTK warned:
   `Gtk-WARNING: Finalizing GtkButton ..., but it still has children left: - GtkPopover`.
3. Break in `on_dialog_response` and press Escape instead of a button.
   Read `response`, and compare it with `GTK_RESPONSE_CANCEL` and
   `GTK_RESPONSE_ACCEPT`.
4. Add a `g_timeout_add_seconds` in `on_activate` whose callback prints a
   counter with `g_print` and returns `G_SOURCE_CONTINUE`. Open the dialog
   and watch the terminal while it is up: that is "modal is not blocking".

![The popover, open under its button.](images/lab10-popover.png){width=55%}

![The modal question.](images/lab10-dialog.png){width=55%}

![After Apply: the answer arrived in the response handler.](images/lab10-confirmed.png){width=55%}

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
