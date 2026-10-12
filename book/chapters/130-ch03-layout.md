# Chapter 3 — Layout and containers

This chapter gives gtklab its shape: a header bar, a sidebar for sessions on
the left, and on the right either a "No tabs open" hint or a notebook of tabs.
Nothing in it does anything yet. The point is to learn how GTK 4 decides
where things go and how big they are, because every layout bug you will meet
later is one of a handful of mistakes about that.

## The idea: a tree of containers, each with one rule

A GTK window holds one child. That child is usually a container, which holds
children of its own, and so on down to buttons and labels. Each container has
one layout rule:

- **`GtkBox`** stacks children in a row or a column.
- **`GtkPaned`** splits its space between two children with a draggable
  divider.
- **`GtkStack`** holds several named children and shows one at a time.
- **`GtkScrolledWindow`** shows one child through a scrollable viewport.
- **`GtkNotebook`** is a stack with tabs.
- **`GtkHeaderBar`** replaces the window's title bar and holds buttons.

![gtklab's widget tree at the end of this chapter. The stack shows one of its two pages at a time.](diagrams/layout-tree.svg)

Sizes are negotiated in two passes. First every widget is **measured**: asked
for its minimum and natural size. Then every widget is **allocated** the
space its parent decided to give it. A widget gets more than its natural
size only if it asks to expand: `hexpand` and `vexpand` say "give me spare
space". Expansion propagates upward, so a widget deep in the tree that wants
to expand makes its ancestors expand too.

`GtkScrolledWindow` is where people get caught most. Its natural height is
small, because the point of a scrolled window is that its content can be
larger than it. Put one in a vertical box without `vexpand`, and it gets its
natural height, which is a few pixels. Chapter 5's Break it shows exactly
that.

GTK 4 widgets are visible from the moment they are created. There is no
`gtk_widget_show_all`. You hide a widget with `gtk_widget_set_visible(w, FALSE)`
and show it again with `TRUE`.

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

::: {.box title="Check yourself"}
Why is a stack better than hiding and showing the notebook? What decides which
child gets extra width? What do `resize` and `shrink` mean for a paned's
child?
:::

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
