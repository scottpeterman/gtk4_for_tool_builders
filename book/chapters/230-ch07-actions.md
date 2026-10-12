# Chapter 7 — Actions, menus and shortcuts

Every command in a tool should be reachable three ways: from a menu, from a
keyboard shortcut, and sometimes from a button. GTK 4 makes that one piece of
code instead of three, by putting every command behind a named **action**.
This chapter adds actions for reload, quit, showing the sidebar and choosing
light or dark, a menu that names them, and shortcuts that reach them first.

::: {.box title="Recall"}
When does `create_child_model` run (Chapter 6)? What is the order of
`setup`, `bind` and `unbind` (Chapter 5)? What survives a rebuild of the
tree, and how?
:::

::: {.box title="In this chapter"}
- Put each command behind one named action and reach it from a menu, a
  shortcut and a button.
- Choose where an action lives (`app.`, `win.` or a widget's own group) and
  debug a name that does not resolve.
- Model toggles and choices as stateful actions, and know when your code
  must set the state itself.
- Explain which code sees a key press first, and register shortcuts so they
  reach the action before the focused widget.
- Save settings once after a burst of changes, with a timeout on the main
  loop.
:::

## Concepts

### Commands, not callbacks

Imagine Reload as a menu item, a shortcut and a toolbar button, each with its
own handler. Sooner or later one of them forgets to keep the open folders,
or to clear the error banner, and the three ways in no longer mean the same
thing. The fix is to give the command a name and make every control refer to
the name.

A **`GAction`** is a named command, optionally with a parameter and a state.
`GSimpleAction` is the implementation you use: you give it a name and a
callback. Actions live in an action map, and both `GtkApplication` and
`GtkApplicationWindow` are action maps. An action added to the window is
addressed as `win.name`; one added to the application, as `app.name`.

![One action, three ways in. Menu items, keyboard shortcuts and buttons all name the same action; the action runs one callback.](diagrams/actions.svg)

The controls do not hold a pointer to your code, only a string. That is what
lets the menu be built from data, the shortcut table live in one array, and a
button in a later chapter reach `win.reload` without knowing that
`AppWindow` exists. It is Chapter 4's boundary again: the control asks for
work by name; the action does it.

### Where an action lives

A widget with an action name looks for it on itself, then on its parent, and
so on up to the window. A `GtkApplicationWindow` answers for the `win.`
prefix, and above the window the application answers for `app.`. If nothing
answers, the control is made insensitive, and nothing is printed.

![How `win.reload` is found. The name is looked up from the control upwards; a name nobody answers greys the control out, silently.](diagrams/ch07-lookup.svg)

The prefix is a scope, and choosing it is a design decision:

| Added to | Prefix | Lives as long as | Fits |
|---|---|---|---|
| the `GtkApplication` | `app.` | the process | commands that need no window: new window, about |
| the `GtkApplicationWindow` | `win.` | that window | commands on one window's state: everything in gtklab |
| any widget, with `gtk_widget_insert_action_group` | your own, e.g. `row.` | that widget | commands for one part of the window, reachable only from inside it |

gtklab puts every action on the window, even Quit, because every callback
needs the `AppWindow` it receives as user data: the sidebar to hide, the
tree to reload, the settings to save. An `app.` action would first have to
work out which window it meant. The isolated lab shows the other choice: its
`app.refresh` holds a pointer to a label in one window, which is fine until
there are two windows.

### Parameter, state and what the menu draws

An action can take a **parameter** (what to do this time) and carry a
**state** (what is true now). The combination decides how a menu draws the
item:

| Shape | Parameter | State | Menu draws | gtklab |
|---|---|---|---|---|
| command | none | none | a plain item | `win.quit`, `win.reload` |
| toggle | none | boolean | a check mark when true | `win.toggle-sidebar` |
| choice | string (`"s"`) | string | a radio dot on the item whose target equals the state | `win.app-variant` |

A choice is one action with several menu items. Each item names it with a
target, `win.app-variant::dark`, and activating that item asks for the state
`'dark'`.

Activating and changing state are separate steps. When a `GSimpleAction`
has no `activate` handler, activating a boolean toggle asks for the opposite
state, and activating a choice asks for the state named by the parameter;
either way the request arrives at `change-state`. If you connect
`change-state`, the request is all you get: the state changes only if your
handler sets it. A short GIO program confirms all three: a handler that
forgot to set the state left it unchanged. gtklab relies on this default
path, which is why its toggle and choice actions have no `activate`
callback. The lab takes the other route: it connects `activate` and flips the
state itself.

**Enabled** is the third property every action has. A disabled action
cannot be activated, and every control that names it greys out together,
which is how a tool says "not now" once instead of three times.

### Menus are models

A **`GMenu`** is not a widget. It is a data model of items, sections and
submenus, where each item is a label and the name of an action. A widget
such as `GtkMenuButton` or `GtkPopoverMenu` turns that model into something
you can click. Because the menu only names actions, the menu item's
sensitivity, check mark or radio dot comes from the action itself: disable
the action and the item greys out; change its state and the check mark
moves. This is Chapter 5's split once more: a model describing what exists,
and a widget that draws it.

### Where shortcuts are caught

A key press goes to the window first. In the capture phase the window, then
each ancestor of the focused widget, gets a chance at it before the focused
widget does (Chapter 9 covers phases properly). Shortcuts handled there never
reach the widget.

![The path of a shortcut. Both of gtklab's registrations sit on the window in the capture phase; a key they match runs its action once and goes no further.](diagrams/ch07-keypath.svg)

gtklab registers each shortcut twice, from one table.
`gtk_application_set_accels_for_action` is what makes menus display
"Shift+Ctrl+B" next to the item, and it also makes the key work: GTK puts
the application's accelerators on each window in the capture phase.
Measured on GTK 4.8.3 and 4.14, a focused widget that swallowed every key
never saw Ctrl+R when the action was registered only this way. The
`GtkShortcutController` that gtklab adds to the window, also in the capture
phase, is the second registration. A key press still runs the action once,
because the first controller that handles the key stops it.

What the controller buys is control. It belongs to the window, it is built
in your code where you can read it, and it does not have to match what the
menu shows: tmssh gives zoom several keys in its controller and shows only
one of them in the menu. The ordering matters from Chapter 13, when the
focused widget is a terminal that would otherwise turn your shortcut into
bytes for the remote end. It also has a cost: a shortcut on a key a widget
needs is stolen from that widget. gtklab's shortcuts all use Ctrl+Shift,
which leaves plain Ctrl keys such as Ctrl+C to the widget underneath.

### Saving without thrashing

A toggle that writes the settings file on every press does three writes for
three presses. A debounce writes once: each change restarts a short timer,
and only the timer that survives a quiet period runs the save.

![Three toggles, one save. Each change removes the pending timeout and starts a new one; only the last fires.](diagrams/ch07-debounce.svg)

The timer holds a pointer to the `AppWindow`. That makes it a lifetime
question in
the Chapter 2 sense: whoever starts a source that points at an object must
remove it before the object dies.

## Build

### One table of shortcuts

```{.cpp include="ch07-actions/src/app_window.cpp" fn="struct Accel {"}
```

```{.cpp include="ch07-actions/src/app_window.cpp" fn="static const Accel kAccels[]" nocomment="1"}
```

### Installing the actions

`GActionEntry` is a table of actions to create in one call. Each entry gives
the name, the activate callback, the parameter type (a GVariant type string,
`"s"` for a string), the initial state (in GVariant text format, so a string
is written `"'light'"` with its own quotes) and the change-state callback:

```{.cpp include="ch07-actions/src/app_window.cpp" fn="void AppWindow::install_actions("}
```

### Stateful actions

A `change_state` callback must set the state itself, or nothing changes and
the check mark never moves:

```{.cpp include="ch07-actions/src/app_window.cpp" fn="void AppWindow::act_toggle_sidebar("}
```

```{.cpp include="ch07-actions/src/app_window.cpp" fn="void AppWindow::act_app_variant("}
```

Dark and light are a GTK setting, not a CSS class. The window asks for the
dark variant of the theme:

```{.cpp include="ch07-actions/src/app_window.cpp" fn="void AppWindow::set_app_variant("}
```

### The menu

```{.cpp include="ch07-actions/src/app_window.cpp" fn="GMenuModel *AppWindow::build_menu("}
```

`build_ui` puts it on a menu button in the header bar.
`gtk_menu_button_set_menu_model` takes its own reference, so `build_ui`
releases the one `build_menu` returned:

```{.cpp include="ch07-actions/src/app_window.cpp" from="GtkWidget *menu_button = gtk_menu_button_new();" to="gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menu_button);"}
```

The Flip button from Chapter 3 is gone.

### A save that waits for the burst to end

Toggling the sidebar three times quickly should write the settings file once,
not three times. Every change restarts a 500 ms timer, and only the timer
that survives writes. This is the main loop strand coming back: a timeout is
a source on the main loop, like the click that triggered it.

```{.cpp include="ch07-actions/src/app_window.cpp" fn="void AppWindow::save_settings_soon("}
```

```{.cpp include="ch07-actions/src/app_window.cpp" fn="gboolean AppWindow::on_save_timeout("}
```

A pending timeout holds `this`, so the destructor removes it. Returning
`G_SOURCE_REMOVE` destroys the source, which is why `on_save_timeout` forgets
its id first: removing a source that has already gone is a critical warning.

## Done when

The menu shows each item's shortcut, Ctrl+Shift+B shows and hides the
sidebar, Dark sticks across a restart, and three quick toggles write the file
once. In the recorded run, three toggles 100 ms apart produced a single
`settings saved`.

![The menu with its shortcuts, the Appearance submenu, and Dark applied.](images/ch07-actions.png)

::: {.box title="Debugger drill"}
Set a breakpoint in `act_app_variant` and choose Dark from the menu. The
requested value arrives as the `GVariant` parameter, already a string; the
action's current state is still the old one until your code sets it. Step
over `g_simple_action_set_state` and open the menu again: the dot has moved.
:::

::: {.box title="Break it"}
In `build_menu`, name the sidebar item's action `toggle-sidebar` without the
`win.` prefix. The item turns grey, with no warning of any kind: the menu
looked for an action called `toggle-sidebar` in a map that does not exist and
concluded it was disabled. When a menu item greys out for no reason, check
the prefix first.
:::

![The Break it: Show Sidebar greyed out by a missing prefix.](images/ch07-breakit-grey.png){width=35%}

## Common mistakes

**Forgetting to set the state in `change_state`.** The callback is a request;
the state changes only if you change it.

**Writing a string state without its quotes.** The state is parsed as
GVariant text, so a string needs its own quotes: `"'light'"`, not `"light"`.

**A shortcut on a key a widget needs.** Application accelerators and the
capture-phase controller both run on the window before the focused widget
sees the key. Bind a plain letter and the search entry can no longer type
it; bind Ctrl+C and, from Chapter 13, the terminal never receives it.

**A timeout that outlives its object.** Any `g_timeout_add` or `g_idle_add`
that holds `this` must be removed in the destructor.

::: {.box title="Read in tmssh"}
`install_actions`, including the `accels` table; `build_menu`, whose theme
list is built from data; and `save_settings_soon`. Note `act_log` and
`sync_log_action` for a check item whose state follows the current tab;
gtklab does the same in Chapter 14.
:::

## Review

1. What is the difference between an action's parameter and its state?
   Give one gtklab action that has only a state and one that has both.
2. A menu item is grey and nothing is printed. List the causes you would
   check, in order.
3. Remove `g_simple_action_set_state` from `act_toggle_sidebar`. Describe
   what the first and the second Ctrl+Shift+B do to the sidebar and to the
   menu's check mark.
4. Why is the initial state of `app-variant` written `"'light'"` in the
   `GActionEntry` table, and what is `"light"` without the inner quotes?
5. Every gtklab action is a `win.` action, Quit included. What would an
   `app.quit` have to do that `win.quit` does not?
6. gtklab registers each shortcut twice. Which registration makes the menu
   show the key, which one runs the action, and why does a key press not run
   it twice?
7. Why does `~AppWindow` remove `save_source_`, and why does
   `on_save_timeout` set it to 0 before returning `G_SOURCE_REMOVE`?
8. The lab's `activate` adds `app.refresh` with a pointer to its window's
   status label. What happens to Refresh if the application opens a second
   window with the same code?

**Questions for deeper thought.** A tool has a Connect command that only
makes sense when a session is selected. Is "disabled" the right answer, or
should the command explain why it cannot run? Which commands in a tool you
use daily belong to the application, which to a window, and which to one
row or tab? A terminal needs Ctrl+C, Ctrl+D and Ctrl+W; which keys are left
for the tool's own shortcuts?

## Exercises

1. **Collapse all.** Add a `win.collapse-all` action in `install_actions`
   in `ch07-actions`, a "Collapse All" item in `build_menu`'s top section and
   `<Control><Shift>k` in `kAccels`. Done when the menu shows the key
   and both the item and the key close every folder.
2. **Not now.** In `load_sessions_file`, disable `win.reload` when the
   sessions file could not be read and enable it when it could
   (`g_simple_action_set_enabled`). Done when, with the file made
   unreadable, the menu item is grey and Ctrl+Shift+R does nothing.
3. **A button for the same command.** Add a Reload button to the header bar
   in `build_ui` with `gtk_actionable_set_action_name(..., "win.reload")`.
   Done when the button, the menu item and Ctrl+Shift+R all stop at one
   breakpoint in `act_reload`, and the button greys out with the menu item
   when Exercise 2 disables the action.
4. **Debounce the divider.** Connect `notify::position` on `paned_` to a
   handler that stores the position in `settings_` and calls
   `save_settings_soon`. Done when dragging the divider across the window
   writes the settings file once, after you let go and wait half a second.

## Isolated lab

gtklab's actions are wired to a tree, a settings file and a debounced save.
The lab keeps only the wiring: one stateless `app.refresh` and one stateful
`win.details`, reached from two buttons, a menu and Ctrl+R. It is a separate
program so that the one thing on the screen is the claim of this chapter:
several controls, one command.

```{.cpp include="labs/lab07-actions/src/main.cpp"}
```

`on_toggle` reads the state with `g_action_get_state`, which returns a new
reference; the lab releases it with `g_variant_unref` before setting the new
state.

Build and run from `labs/lab07-actions`:

```
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/lab
```

In the recorded run, the Refresh button and Ctrl+R each printed
`app.refresh activated`, Toggle details printed `win.details state: true`,
and the menu showed "Refresh Ctrl+R" and a checked "Toggle details".

| Stop at | What it shows |
|---|---|
| `on_refresh` | reached from the button, the menu item and Ctrl+R; run `bt` each time and find where the three stacks join |
| `on_toggle` | `state` holding the old value; the new one exists only after `g_simple_action_set_state` |
| `g_simple_action_set_state` | the menu's check mark and the label both follow from this one call |

Experiments:

1. **Disable the command.** Call `g_simple_action_set_enabled(refresh,
   FALSE)` before the unref. In the recorded run the Refresh button and the
   Refresh menu item were both grey, and Ctrl+R printed nothing.
2. **Drop the prefix.** Set the details button's action name to `details`.
   The button was grey and the terminal said nothing: no control above it
   answers an unprefixed name.
3. **Let GIO toggle.** Replace the `activate` connection with a
   `change-state` handler that calls `g_simple_action_set_state(action,
   value)` and updates the label from `value`. Clicking Toggle details
   twice printed `true` then `false`: with no `activate` handler, the action
   flipped its boolean and asked for the change itself.
4. **Steal a key.** Change the accelerator to plain `r` and add a
   `GtkEntry` above the buttons. Typing `error` into the entry left `eo` in
   it and printed `app.refresh activated` three times: the application's
   accelerators ran before the focused entry saw the key.

![Lab 7 after Toggle details.](images/lab07-actions.png){width=55%}

![The lab's menu: the shortcut comes from the application's accelerators, the check mark from the action's state.](images/lab07-menu.png){width=55%}

# Checkpoint B — Session browser {.unnumbered}

**Spec.** Chapter 7, plus:

- an **Edit Sessions File** action (Ctrl+Shift+E) that opens the sessions file
  in the user's editor; `gtk_show_uri` is the 4.8 API for it;
- a red banner above the tree when the sessions file is missing or malformed,
  giving the reason, with the last good tree kept underneath it rather than
  emptied;
- Reload (Ctrl+Shift+R) that re-reads the file without collapsing open
  folders.

**Done when.** Editing the file and pressing Ctrl+Shift+R updates the tree
with open folders still open; a `port=abc` line shows the banner; fixing the
file and reloading clears it. In the recorded run the banner read
"Sessions file: session 'broken': port is not a number".

![Checkpoint B: a session added and reloaded; a bad port shows the banner over the last good tree; fixed and reloaded.](images/ckB-banner.png)

**Hints.** The banner is a `GtkLabel` with the `error` style class, which the
theme colours red, and wrapping turned on. Hide it with
`gtk_widget_set_visible` when the file is good. Keep the last good folders by
loading into a temporary and swapping only on success.

**Reference.** `ckB-browser/`.
