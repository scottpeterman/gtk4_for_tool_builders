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

## The idea: controllers, phases and claiming

GTK 4 widgets have no event handler functions to override. Input is handled
by **event controllers** that you attach to widgets with
`gtk_widget_add_controller`. Each controller handles one kind of input:

| Controller | Handles |
| --- | --- |
| `GtkGestureClick` | presses and releases, single or multiple clicks |
| `GtkGestureDrag` | press, move, release, with the offset |
| `GtkEventControllerKey` | key presses and releases |
| `GtkEventControllerScroll` | wheel and touchpad scrolling |
| `GtkEventControllerMotion` | pointer enter, leave and motion |
| `GtkEventControllerFocus` | keyboard focus in and out |
| `GtkShortcutController` | keyboard shortcuts mapped to actions |

When an event arrives, GTK finds the target widget (the one under the
pointer, or with keyboard focus) and delivers the event in three phases.
**Capture** runs from the window down to the target, giving each ancestor a
chance first. **Target** runs on the target itself. **Bubble** runs from the
target back up to the window. A controller runs in the phase you set with
`gtk_event_controller_set_propagation_phase`; the default is bubble.

![The three phases for a click on the text view inside a tab. Capture-phase controllers on the page run before anything inside it; bubble-phase controllers run only if the event is still unhandled on the way back up.](diagrams/propagation.svg)

How a controller stops an event depends on its kind. A **gesture** decides
about the whole press-move-release sequence: setting its state to
`GTK_EVENT_SEQUENCE_CLAIMED` takes the sequence, and no other gesture sees
it; `GTK_EVENT_SEQUENCE_DENIED` steps aside, and the event carries on. A
**discrete** controller such as scroll or key returns a boolean from its
signal: `TRUE` means handled, stop here; `FALSE` means not mine, carry on.

So the pattern for "the page gets first refusal" is a controller on the page
in the capture phase, which claims what it wants and denies or returns
`FALSE` for everything else.

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

::: {.box title="Check yourself"}
What are the three phases, and in which direction does each travel? What
does claiming a sequence do? What happens after a scroll handler returns
`FALSE`?
:::
