# Chapter 3 — Layout and containers

This chapter gives gtklab its shape: a header bar, a sidebar for sessions on
the left, and on the right either a "No tabs open" hint or a notebook of tabs.
Nothing in it does anything yet. The point is to learn how GTK 4 decides
where things go and how big they are, because every layout bug you will meet
later is one of a handful of mistakes about that.

::: {.box title="In this chapter"}
- Read a window as a tree of containers and predict each container's share
  of the space.
- Explain the measure-then-allocate negotiation, and use `hexpand`,
  `vexpand` and alignment to steer it.
- Choose between a box, a paned, a stack and a notebook for a region of a
  tool's window.
- Diagnose the common layout failures: a collapsed scrolled window, a
  sidebar dragged out of existence, a stack sized by a page you cannot see.
:::

## Concepts

### A tree of containers, each with one rule

A GUI toolkit could let you place controls at coordinates. GTK does not. You
describe a tree of widgets, each container in it applies one layout rule to
its children, and GTK works out positions and sizes again every time the
window changes. A sidebar never needs to know where the content area
starts; it only takes part in its parent's rule.

A GTK window holds one child. That child is usually a container, which holds
children of its own, and so on down to buttons and labels. There is no
generic "add" call: each container has its own, because each has its own
idea of what a child is.

| Container | Its rule | Add a child with | Watch for |
|---|---|---|---|
| `GtkBox` | children in a row or a column | `gtk_box_append` | spacing is between children; margins belong to each child |
| `GtkPaned` | two children and a draggable divider | `gtk_paned_set_start_child`, `..._end_child` | by default either child can be squeezed to nothing |
| `GtkStack` | several named children, one visible | `gtk_stack_add_named` | by default sized for its largest page, visible or not |
| `GtkScrolledWindow` | one child seen through a viewport | `gtk_scrolled_window_set_child` | its natural height is small |
| `GtkNotebook` | pages with a tab each | `gtk_notebook_append_page` | the tab row is its own widget, measured like any other |
| `GtkHeaderBar` | buttons in the title bar | `gtk_header_bar_pack_start`, `..._end` | it goes in the window's titlebar slot, not its child |

A window has two slots, not one. `gtk_window_set_titlebar` installs the
header bar where the window manager's title bar would be;
`gtk_window_set_child` installs the content. gtklab's tree hangs the header
bar and the paned off the window side by side:

![gtklab's widget tree at the end of this chapter. The sidebar is the paned's start child and the stack its end child; the stack shows one of its two pages at a time.](diagrams/layout-tree.svg)

The stack and the notebook are two levels of navigation. The stack chooses
what the content area is showing (the empty hint or the tabs); the notebook
chooses which tab within it. Pages in a stack are not created and destroyed
as you switch: they stay in the tree, hidden, which matters both for sizing
and, from Chapter 8, for the state behind each tab.

### Measure, then allocate

Sizes are negotiated in two passes. First every widget is **measured**:
asked for its minimum and natural size. Then every widget is **allocated**
the space its parent decided to give it. Measuring goes up the tree, because
a box cannot know its own size until it knows its children's; allocating
goes down, because a child cannot know its rectangle until its parent has
divided the space. GTK runs both in the layout phase of a frame, so a
change you make in a handler shows up on the next frame, not inside the
call.

![One layout pass. Changes only queue a resize; the frame clock runs measurement and allocation together, then paints.](diagrams/ch03-measure.svg)

Minimum and natural are different promises. Minimum is the smallest size at
which the widget still works; natural is the size it would like. A window
can never be smaller than its child's minimum. Between minimum and natural,
and beyond natural, the parent's rule decides.

### Expand, align, margin, minimum

Four properties on every widget steer the parent's decisions, and they are
easy to confuse:

| Property | Answers | Default |
|---|---|---|
| `hexpand`, `vexpand` | Should this widget get a share of spare space? | no |
| `halign`, `valign` | Where does it sit inside the space it got? | `FILL`: use all of it |
| margins | How much empty space surrounds it? | 0 |
| `gtk_widget_set_size_request` | What is its minimum, at least? | its own measurement |

A widget gets more than its natural size only if it asks to expand.
Expansion propagates upward, so a widget deep in the tree that wants to
expand makes its ancestors expand too: a box holding one `hexpand` label
reports that it wants to expand, though nobody set `hexpand` on the box.
Alignment is separate. It decides what a widget does with more space than
it asked for. A label with `valign` set to `START` in a tall space is given
only its natural height and placed at the top; with the default `FILL` it
would be stretched over the whole height. The sidebar placeholder uses
`START`.

A size request is a floor, not a size. It cannot make a widget smaller than
it needs, and, as the Isolated lab shows, it does not stop a paned from
squeezing the widget anyway.

### Where layouts go wrong

**`GtkScrolledWindow`** is where people get caught most. Its natural height
is not its content's: measured with eight lines of text inside, a scrolled
window reported the same small value for minimum and natural, because the
point of a scrolled window is that its content can be larger than it. Put
one in a vertical box without `vexpand`, and it gets that natural height,
which is a few pixels. Chapter 5's Break it shows exactly that.

**`GtkPaned`** defaults to `resize` and `shrink` both on, for both children:
both grow when the window grows, and either can be dragged below its
minimum size until it disappears. For a sidebar both defaults are wrong,
and the Build section turns them off for the start child.

**`GtkStack`** defaults to homogeneous sizing: it is as big as its biggest
page, so it measures every page, including the hidden ones. A hidden page
can make the visible one sit in a space too wide for the pane, or, as the
Build section shows, make GTK warn about a page nobody can see.

### Layout is not ownership

Chapter 2's rule still holds: a container owns its children, and removing a
child destroys it. But ownership says how long a widget lives, and layout
says where it is and how big. Hiding a widget with
`gtk_widget_set_visible(w, FALSE)` changes its layout (it takes no space)
and leaves its lifetime alone; removing it ends both. GTK 4 widgets are
visible from the moment they are created, so there is no
`gtk_widget_show_all`, and `gtk_widget_set_visible(w, TRUE)` brings a
hidden widget back.

The Flip button's handler gets the stack as `user_data`, a borrowed pointer.
That is safe here for a reason worth naming: the button and the stack are
in the same window and are destroyed together. A pointer from one window's
widget to another window's would need the care Chapter 2 described.

## Build

The layout code is in three functions, and the whole `main.cpp` is short
enough to read in full:

```{.cpp include="ch03-layout/src/main.cpp"}
```

The paned's two settings for its start child work together.
`gtk_paned_set_resize_start_child(paned, FALSE)` means that when the window
grows, the extra space goes to the content, not the sidebar.
`gtk_paned_set_shrink_start_child(paned, FALSE)` means the divider cannot be
dragged past the sidebar's minimum width.

The stack has homogeneous sizing turned off in both directions. By default a
stack is as big as its biggest page, so it measures every page even when one
is hidden. That sounds harmless, and is, until a page is a notebook with no
tabs: GTK 4.14 then logs
`GtkGizmo (tabs) reported min height -3, but sizes must be >= 0` every time
the last tab closes. Measuring only the visible page avoids it.

The Flip button is temporary scaffolding; Chapter 7 replaces it with an
action.

## Done when

The window has a header bar with a Flip button, the sidebar holds a search
entry, the Flip button switches between the hint and the notebook, and the
divider cannot be dragged so far left that the sidebar disappears.

![Left: the empty state. Middle: Flip shows the notebook. Right: the divider dragged as far left as it will go; the sidebar stops at its minimum width.](images/ch03-layout.png)

::: {.box title="Debugger drill"}
Run **gtklab + GTK Inspector**. In the Inspector's object tree, find the
`GtkPaned` and change its `position` property: the divider moves as you type.
Select the `GtkStack` and change `visible-child-name` to `tabs` by hand. Then
select the search entry and look at its allocation and its `hexpand` and
`vexpand` values. Everything `on_activate` built is there, live.
:::

::: {.box title="Break it"}
Delete the `gtk_paned_set_shrink_start_child(..., FALSE)` line and drag the
divider fully left. The sidebar collapses to nothing, search entry and all,
and there is no way to tell from the window that it was ever there.
:::

![The Break it: with shrinking allowed, the divider swallows the sidebar.](images/ch03-breakit-collapsed.png){width=45%}

## Common mistakes

**Looking for `gtk_widget_show_all`.** It is gone, and not needed.

**Calling `gtk_container_add`.** Gone too. Each container has its own call:
`gtk_box_append`, `gtk_window_set_child`, `gtk_paned_set_start_child`,
`gtk_scrolled_window_set_child`.

**A scrolled window without `vexpand`.** It gets its natural height, which is
tiny.

**Setting sizes in pixels.** `gtk_widget_set_size_request` sets a minimum,
not a size, and fights the user's font size. Prefer expansion and alignment.

::: {.box title="Read in tmssh"}
`build_ui`, which creates the header bar, paned, stack and notebook, and
`build_sidebar`, which builds the search entry, an error banner and the
session list inside a scrolled window.
:::

## Review

1. Why does GTK measure from the bottom of the tree up but allocate from the
   top down?
2. A label in a vertical box has `vexpand` set and `valign` set to `START`.
   How much space does it get, and where does its text appear?
3. Nobody calls `gtk_widget_set_vexpand` on gtklab's sidebar box. Why does
   it still report that it wants spare vertical space?
4. A scrolled window in a vertical box shows a strip a few pixels high. What
   is its natural height based on, and what is the one-line fix?
5. Why is a stack better than removing the notebook and adding a label in
   its place when the last tab closes?
6. The stack's hidden page is wider than the pane. With homogeneous sizing
   on, what happens to the visible page, and why?
7. What do `resize` and `shrink` mean for a paned's start child, and why are
   the defaults wrong for a sidebar?
8. `gtk_widget_set_size_request(sidebar, 170, -1)` is set, yet the sidebar
   can still be dragged to nothing. Why does the size request not prevent it?

**Questions for deeper thought.** A tool's window will be used on a laptop
screen and on a wide monitor. Which regions of your layout should absorb
extra space, which should hold steady, and which should the user control?
When would you let the user hide a region entirely, and how would they get
it back if the divider is the only handle?

## Exercises

1. **A footer button.** In `build_sidebar` in `ch03-layout/src/main.cpp`,
   append a "New session" button after the scroller. Done when the button
   stays pinned to the bottom of the sidebar as the window grows taller.
2. **A third page.** In `build_content`, add a third stack page named
   `settings` holding a label, and change `on_flip_clicked` to cycle
   `empty`, `tabs`, `settings`. Done when three clicks of Flip return to the
   hint.
3. **Hide the sidebar.** In `on_activate`, add a second header-bar button
   whose handler toggles the sidebar with `gtk_widget_set_visible`. Done
   when the content takes the full width while the sidebar is hidden and
   the sidebar comes back at its old width.
4. **Animate the switch.** In `build_content`, call
   `gtk_stack_set_transition_type` with
   `GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT`. Done when Flip slides
   between the pages instead of cutting.

## Isolated lab

gtklab's window has a header bar, a paned, a sidebar and a stack in it, and
all of them take part in layout. The lab strips that down to the three
decisions this chapter is about: a box that stacks a toggle button over a
paned, a paned with a fixed-purpose sidebar, and a stack with two pages.
Every container keeps its default settings except the one `vexpand`, so
what you see is GTK's default policy, and each experiment changes one line.

```{.cpp include="labs/lab03-layout/src/main.cpp"}
```

The sidebar has a 170-pixel size request and the paned starts at 190, so
the divider starts just right of the sidebar's minimum. The stack is left
homogeneous, unlike gtklab's.

Build and run from `labs/lab03-layout`:

```
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/lab
```

Or open the directory in VS Code and use the **lab** launch
configuration; **lab + GTK Inspector** is the one for this lab. Clicking
**Show details** prints `Switching to page: details` and shows the second
page.

| Stop at | What it shows |
|---|---|
| `on_activate`, after `gtk_paned_set_position` | the whole tree is built and nothing is on screen: layout has not run yet |
| `on_toggled` | `active` is the toggle's new state; `user_data` is the stack, a borrowed pointer |
| `gtk_stack_set_visible_child_name` (gdb, with debuginfod) | GTK changing the visible child and queueing a resize, not laying out on the spot |
| Inspector: select the `GtkPaned` | its `position`, and the `shrink-start-child` and `resize-start-child` values the lab never set |

Experiments:

1. **Drag the divider fully left.** The sidebar collapses to nothing,
   170-pixel size request and all, because `shrink-start-child` defaults to
   on. Then add `gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE)`
   and drag again: the divider stops at the sidebar's minimum.
2. **Remove `gtk_widget_set_vexpand(paned, TRUE)`.** The paned shrinks to a
   single line under the toggle and the rest of the window is empty. The
   root box gives spare space only to children that ask for it.
3. **Make a hidden page wide.** Change the details label to a sentence
   longer than the pane is wide and run without clicking anything. "Welcome
   page" is no longer centred: it sits right of centre, because the
   homogeneous stack is allocated the hidden page's minimum width and the
   pane shows only the left part of it. Add
   `gtk_stack_set_hhomogeneous(GTK_STACK(stack), FALSE)` and "Welcome page"
   is centred again.
4. **Resize the window wider.** With the defaults, both sides of the paned
   grow. Add `gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE)`
   and only the stack grows.

![Lab 3 after Show details: the toggle above, the sidebar and the stack's details page below.](images/lab03-layout.png){width=70%}

# Checkpoint A — A shell that remembers {.unnumbered}

**Spec.** Chapter 3's window, plus its size saved when it closes and restored
when it opens. Store it in a `GKeyFile` at
`g_get_user_config_dir()/gtklab/settings.ini`, creating the directory with
`g_mkdir_with_parents`. Save in `close-request`; load before
`gtk_window_set_default_size`.

**Done when.** You resize the window, quit and relaunch, and it comes back at
the same size. Deleting the settings file brings back 900 by 600 with no
warning printed. In the recorded test the window was resized to 1100 by 700,
the file read `width=1100` and `height=700`, and the relaunched window
measured 1100 by 700; with the file deleted it opened at 900 by 600.

**Hints.** `g_key_file_get_integer` reports a missing key through its
`GError`; treat that as "use the default", not as a failure. When you save,
load the existing file first with `G_KEY_FILE_KEEP_COMMENTS`, so saving the
size does not erase other settings later chapters add.

**Reference.** `ckA-remember/src/main.cpp`. Compare it with tmssh's
`on_close_request` and the settings load in the `AppWindow` constructor.
Chapter 4 moves settings out of GTK code entirely.
