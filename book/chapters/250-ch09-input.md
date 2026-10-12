# Chapter 9 — Event controllers and propagation

A right-click on a tab should open gtklab's menu, and Ctrl+wheel should zoom,
even though the click and the scroll land on a text view that has its own
ideas about both. Getting that right needs the one part of GTK 4 that is
most different from GTK 3, and from most other toolkits: how input reaches
widgets.

::: {.box title="Recall"}
In what order do `page-removed` and `~Tab` run (Chapter 8)? Which phase did
the Chapter 7 shortcut controller use, and why?
:::

::: {.box title="In this chapter"}
- Pick the controller for a kind of input and attach it to the right widget.
- Predict which controllers see an event, and in what order, from the
  widget tree and each controller's phase.
- Stop an event deliberately: claim or deny a gesture, return `TRUE` or
  `FALSE` from a discrete handler.
- Give a container first refusal over input without breaking the widgets
  inside it.
:::

## Concepts

### No event handlers, only controllers

GTK 3 code handled input by connecting to signals every widget had,
`button-press-event`, `key-press-event`, `scroll-event`, and returning
`TRUE` to stop the event. A widget that wanted input also had to ask for it
with an event mask. GTK 4 removed all of it. A program that looked those
signals up on `GtkWidget` got signal id 0 for each one, the 4.8 headers have
no `gtk_widget_add_events`, and `GtkWidgetClass` has no event functions to
override.

| GTK 3 | GTK 4 |
| --- | --- |
| `button-press-event` on the widget | a `GtkGestureClick` added to the widget |
| `scroll-event`, return `TRUE` to stop | `GtkEventControllerScroll`, return `TRUE` to stop |
| `key-press-event` | `GtkEventControllerKey`, or a `GtkShortcutController` for shortcuts |
| an event mask before anything arrives | nothing: a controller gets the input it handles |
| override a class's event function | add a controller in the widget's init or where it is built |

Input is handled by **event controllers** that you attach with
`gtk_widget_add_controller`, which takes ownership of the controller. A
controller belongs to one widget and dies with it. Each controller handles
one kind of input:

| Controller | Handles |
| --- | --- |
| `GtkGestureClick` | presses and releases, single or multiple clicks |
| `GtkGestureDrag` | press, move, release, with the offset |
| `GtkEventControllerKey` | key presses and releases |
| `GtkEventControllerScroll` | wheel and touchpad scrolling |
| `GtkEventControllerMotion` | pointer enter, leave and motion |
| `GtkEventControllerFocus` | keyboard focus in and out |
| `GtkShortcutController` | keyboard shortcuts mapped to actions |

Two defaults are worth knowing because they bite. A new click gesture
listens to button 1 only (`gtk_gesture_single_get_button` returned 1); pass
0 to `gtk_gesture_single_set_button` to hear every button. And every
controller starts in the bubble phase, including key and shortcut
controllers, which is why Chapter 7 had to set the capture phase on its
shortcut controller explicitly.

### Where an event goes

When an event arrives, GTK first finds its **target**. A pointer event's
target is the widget under the pointer; a key event's target is the widget
with keyboard focus. Then it walks the chain of ancestors between the
window and the target in three phases. **Capture** runs from the window
down to the target, giving each ancestor a chance first. **Target** runs
controllers on the target itself. **Bubble** runs from the target back up
to the window. A controller runs in the phase you set with
`gtk_event_controller_set_propagation_phase`.

![The three phases for a click on the text view inside a tab. Capture-phase controllers on the page run before anything inside it; bubble-phase controllers run only if the event is still unhandled on the way back up.](diagrams/propagation.svg)

A test program with a box holding a label, and a click gesture in each of
the three phases on both, printed this for one click on the label:

```
pressed: parent capture
pressed: child capture
pressed: child target
pressed: child bubble
pressed: parent bubble
```

The box's target-phase gesture never ran: the box was not the target. A
target-phase controller hears only input aimed at its own widget, never at
a child.

### Stopping an event

How a controller stops an event depends on its kind.

![Gestures decide by setting a sequence state; discrete controllers decide by their return value.](diagrams/ch09-handled.svg)

A **gesture** decides about the whole press-move-release sequence. Setting
its state to `GTK_EVENT_SEQUENCE_CLAIMED` takes the sequence, and no other
gesture sees it. `GTK_EVENT_SEQUENCE_DENIED` steps aside, and the event
carries on. In the test program, making the label's bubble gesture claim the
press removed the last line: the box's bubble gesture never ran. A
**discrete** controller such as scroll or key returns a boolean from its
signal: `TRUE` means handled, stop here; `FALSE` means not mine, carry on.

So the pattern for "the page gets first refusal" is a controller on the page
in the capture phase, which claims what it wants and denies or returns
`FALSE` for everything else.

### Choosing a phase

| You want | Phase | Attach to | In gtklab |
| --- | --- | --- | --- |
| First refusal over everything inside a container | capture | the container | the page's click and zoom; the window's shortcuts (Chapter 7) |
| Input on a widget, unless something inside used it | bubble (the default) | the widget | the text view's logging gesture |
| Input aimed at this widget only, not its children | target | the widget | not used |
| A controller switched off for now | none | the widget | not used |

Capture is powerful and easy to overuse. A capture controller that claims a
little too much breaks every widget inside its container, and the widgets
cannot do anything about it. Claim exactly what you handle and let the rest
through. That discipline is what Part III needs: a terminal wants almost
every key and every mouse button, the application wants a few shortcuts and
a right-click menu, and a full-screen program inside the terminal may want
the mouse too. Each of those is a controller, a phase and a claim.

### What the handler is given

A controller's handler gets the controller, the event's details (button,
position, scroll deltas) and your user data. Read the event's modifier keys
with `gtk_event_controller_get_current_event_state` inside the handler; it
is the state for the event being handled, not whatever the keyboard
happens to be doing now.

The user data here is the `Tab`, a plain pointer, with plain
`g_signal_connect`. That is safe for the reason Chapter 8 gave: the
controllers belong to the page, the page owns the `Tab`, and they go
together. A handler whose data had a different owner would need
`g_signal_connect_object` or a disconnect, as Chapter 2 showed.

## Build

In `add_tab`, two capture-phase controllers on the page and one ordinary
bubble-phase gesture on the text view, there only to show when the view gets
a click:

```{.cpp include="ch09-input/src/app_window.cpp" from="// Capture phase on the page: these run before anything inside it." to="gtk_widget_add_controller(tab->view, GTK_EVENT_CONTROLLER(view_click));"}
```

The click gesture listens to every button (`set_button(..., 0)`) and decides
per press. A right-click is claimed, so the view never sees it; Chapter 10
opens a menu there. Anything else is denied, so the view gets it:

```{.cpp include="ch09-input/src/app_window.cpp" fn="void AppWindow::on_page_pressed("}
```

The scroll handler takes Ctrl+wheel and lets a plain wheel through. The
modifier state comes from the event being handled, with
`gtk_event_controller_get_current_event_state`:

```{.cpp include="ch09-input/src/app_window.cpp" fn="gboolean AppWindow::on_zoom_scroll("}
```

For now the zoom level only shows in the tab label; Chapter 14 turns it into
a font size.

## Done when

Left-click still places the cursor in the text view, right-click does not
open the text view's own menu, the wheel scrolls, and Ctrl+wheel changes the
number in the tab label instead of scrolling.

![Ctrl+wheel twice over a tab: the label shows 120%, and the text did not scroll.](images/ch09-zoom.png){width=75%}

::: {.box title="Debugger drill"}
Run the scratch config, so the handlers' `g_debug` lines print, and try the
four inputs in turn. This is the recorded log:

1. Left-click: `page capture: button 1 denied`, then
   `text view (bubble): button 1`.
2. Right-click: `page capture: button 3 claimed`. The text view never sees
   it.
3. Wheel: `zoom scroll: plain wheel, passing it on`, and the text scrolls.
4. Ctrl+wheel: `zoom scroll: ctrl+wheel, zoom 110%`, and nothing scrolls.

That sequence is the propagation model. If you can predict all four lines
before you try them, you understand it. Then set breakpoints in the three
handlers and repeat, to see the phases in the call stacks.
:::

::: {.box title="Break it"}
Change the click gesture's phase to `GTK_PHASE_BUBBLE` and right-click. The
text view's own context menu (Cut, Copy, Paste, Select All, Insert Emoji)
appears instead of your handler. The target handled the click in its own
phase, before it could bubble up to the page.
:::

![The Break it: in the bubble phase, the text view's built-in menu wins.](images/ch09-breakit.png){width=70%}

## Common mistakes

**Claiming everything.** A capture-phase gesture that claims every press
makes the widgets inside the page unusable. Claim only what you handle.

**Returning `TRUE` from a scroll handler for events you ignored.** The scroll
is swallowed and the view never scrolls.

**Reading modifiers from a global.** Use
`gtk_event_controller_get_current_event_state` inside the handler; it is the
state for this event.

**Adding a controller to a widget that has not been created yet.** Add
controllers where the widget is built. A controller belongs to one widget.

::: {.box title="Read in tmssh"}
The page controllers in `add_tab`, `on_terminal_pressed` and
`on_zoom_scroll`. Then gtkpyte's own controllers in `gtkpyte_terminal_init`,
especially `on_report_begin`, a capture-phase drag gesture that claims mouse
input when a full-screen program such as vim wants it and denies it so that
selection works when no program does, or when you hold Shift.
:::

## Review

1. You left-click in a tab's text view. Name the gtklab controllers that
   see the press, in order, and what each one does with it.
2. The page's click gesture claims a right-click. Why does that stop the
   text view's own context menu, and why does the same gesture in the
   bubble phase not?
3. What would the user notice if `on_zoom_scroll` returned `TRUE` for a
   plain wheel too?
4. A key press while the session tree has focus: which widget is the
   target, and why could a shortcut controller on the notebook never see
   it?
5. You add a click gesture to the page in the target phase. Does it hear a
   click on the text view? Why?
6. Why is it safe to pass the `Tab` as plain user data to the page's
   controllers?
7. Why does the zoom handler ask the controller for the modifier state
   instead of tracking Ctrl with a key controller of its own?

**Questions for deeper thought.** A terminal widget wants every key, and
your tool wants Ctrl+Shift+T for a new tab. Which side should win, where
does each controller sit, and how would a user type a key your shortcut
took? A capture-phase gesture on a container is invisible to whoever
writes the widgets inside it; how would you make that policy discoverable
to a teammate? GTK 3 let any widget see any event; what did GTK 4 gain by
making input something you attach rather than something you override?

## Exercises

1. **Shift for the built-in menu.** In `ch09-input/src/app_window.cpp`,
   change `on_page_pressed` to deny a right-click when Shift is held (read
   it with `gtk_event_controller_get_current_event_state`). Done when
   right-click logs `claimed` and Shift+right-click opens the text view's
   own menu. This is the rule gtkpyte uses for selection.
2. **Ctrl+0 resets the zoom.** In `add_tab`, add a `GtkEventControllerKey`
   to the page in the capture phase whose `key-pressed` handler returns
   `TRUE` only for Ctrl+0, setting `tab->zoom` back to 100 and updating the
   label. Done when Ctrl+0 resets the label to 100% and every other key
   still types into the view.
3. **Middle-click closes.** In `add_tab`, add a `GtkGestureClick` for
   button 2 to the tab label box that closes the tab the way
   `on_tab_close_clicked` does. Done when middle-clicking a tab's label
   closes it and prints `~Tab`, and middle-clicking inside the text view
   does not.
4. **Watch the pointer.** Add a `GtkEventControllerMotion` to the page and
   `g_debug` its `enter` and `leave` signals with the session name. Done
   when moving the pointer into the text area and out to the sidebar logs
   one enter and one leave, and switching tabs with the pointer still
   over the content tells you whether the new page gets an enter.

