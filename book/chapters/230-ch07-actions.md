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

## The idea: menus are data, commands are actions

A **`GAction`** is a named command, optionally with a parameter and a state.
`GSimpleAction` is the implementation you use: you give it a name and a
callback. Actions live in an action map, and both `GtkApplication` and
`GtkApplicationWindow` are action maps. An action added to the window is
addressed as `win.name`; one added to the application, as `app.name`.

A **`GMenu`** is not a widget. It is a data model of items, sections and
submenus, where each item is a label and the name of an action. A widget
such as `GtkMenuButton` or `GtkPopoverMenu` turns that model into something
you can click. Because the menu only names actions, the menu item's
sensitivity, check mark or radio dot comes from the action itself: disable
the action and the item greys out; change its state and the check mark
moves.

![One action, three ways in. Menu items, keyboard shortcuts and buttons all name the same action; the action runs one callback.](diagrams/actions.svg)

State turns an action into a toggle or a choice:

- An action with a **boolean state and no parameter** is a check item.
  Activating it from a menu flips the state, and your `change_state`
  callback receives the new value.
- An action with a **string parameter and a string state** is a radio group.
  Each menu item names the action with a target, `win.app-variant::dark`, and
  the item whose target equals the state shows the dot.

Shortcuts are registered twice in gtklab, for two different reasons.
`gtk_application_set_accels_for_action` is what makes menus display
"Shift+Ctrl+B" next to the item. A `GtkShortcutController` on the window in
the **capture** phase is what makes the shortcut fire before the focused
widget sees the key. Chapter 9 explains phases; for now, capture means
"first". That ordering matters from Chapter 13, when the focused widget is a
terminal that would otherwise turn your shortcut into bytes for the remote
end.

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

**Shortcuts that work in menus but not in the window.**
`gtk_application_set_accels_for_action` labels the menu and does trigger the
action, but only after the focused widget has had the key. If that widget
consumes it, nothing happens. That is what the capture-phase controller is
for.

**A timeout that outlives its object.** Any `g_timeout_add` or `g_idle_add`
that holds `this` must be removed in the destructor.

::: {.box title="Read in tmssh"}
`install_actions`, including the `accels` table; `build_menu`, whose theme
list is built from data; and `save_settings_soon`. Note `act_log` and
`sync_log_action` for a check item whose state follows the current tab;
gtklab does the same in Chapter 14.
:::

::: {.box title="Check yourself"}
What is the difference between an action's parameter and its state? Why are
shortcuts registered twice? What does a menu item do when the action it names
does not exist?
:::

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
