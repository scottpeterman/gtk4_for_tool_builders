# Chapter 6 — List views II: trees, expansion, filtering

A flat list of two thousand sessions is not a session manager. This chapter
groups sessions into folders that open and close, adds a search box that
filters as you type, and makes sure that clearing the search puts the tree
back exactly as the user left it. That last requirement is the one that
shapes the code.

::: {.box title="Recall"}
What does `setup` do that `bind` must not (Chapter 5)? Why must list items be
GObjects? Which file holds the session model, and does it include GTK
(Chapter 4)?
:::

::: {.box title="In this chapter"}
- Turn a folder model into an expandable tree with `GtkTreeListModel` and
  `GtkTreeExpander`, reusing Chapter 5's factory.
- Predict when the child-model function runs, and write one that is cheap
  and safe to call again.
- Choose where to filter a tree, and filter in the tested core.
- Rebuild a tree without losing which folders the user had open.
:::

## Concepts

### A tree is a list that grows

GTK 4 has no tree widget. It has `GtkTreeListModel`, a model that turns a
tree into a flat list. You give it a root model, here the folders, and a
function that returns a model of children for an item, or `NULL` for an item
that has none. When a row is expanded, its children are inserted into the
flat list after it; when it is collapsed, they are removed. The list view
underneath neither knows nor cares that it is showing a tree.

![The data is a tree; the view sees a list. Expanding Core inserted its two sessions after it, and Edge moved from position 2 to position 4.](diagrams/ch06-flatten.svg)

Two consequences follow. Expanding and collapsing change the presentation,
not the data: a closed folder still has its sessions, they are just not rows.
And a position is not an identity. Opening a folder shifts every row below
it, so anything that must survive an expand, a rebuild or a reload (the
folders that were open, the session that was selected) has to be remembered
by something stable, such as a name, never by a row number.

### The row, the item and the expander

Three objects with similar names do three different jobs:

| Object | What it is | Question it answers |
|---|---|---|
| `GtkTreeListModel` | the model that flattens the tree and tracks expansion | which rows exist right now? |
| `GtkTreeListRow` | one item of that model, wrapping your item | where is this item: depth, expanded, parent? |
| `GtkTreeExpander` | a widget in your row | how is it drawn: indent, triangle, click to open |

Your own `GtklabSessionItem` is the fourth thing, inside the row. With
`passthrough` set to `FALSE`, the list items the factory receives are
`GtkTreeListRow`s, and you unwrap yours with `gtk_tree_list_row_get_item`,
which returns a new reference. The expander gets the row in `bind`, and
loses it in `unbind`, so a recycled row widget never shows the previous
row's indent or triangle. Chapter 5's discipline is unchanged; the row
widget just has one more layer.

With `autoexpand` set to `TRUE`, every folder opens as soon as it appears.
gtklab passes `FALSE`: the user decides what is open, and the code below
spends most of its effort respecting that.

### When the child function runs

![The model chain for a tree. The tree list model calls `create_child_model` for folder rows, and inserts the children after the folder when it is expanded.](diagrams/tree-model-chain.svg)

The usual guess is that the child function runs when a folder is opened.
It runs earlier and more often than that. An expander has to decide whether
to draw a triangle before anyone clicks it, so it asks the tree model
whether the row can expand, and the tree model answers by calling your
function. In GTK 4.8's source, the model built to answer that question is
released straight away; expanding the row calls the function again. The isolated lab measures it: both folders' child functions run
at startup, before any click, and every leaf's runs as its row is bound.
Remove the call that hands the row to the expander and the function is never
called at all.

The contract, from GTK's own header: return `NULL` for an item that can
never have children, and a model for one that can. An item that has no
children yet but may get them later should return an empty model and fill it
when they arrive. That is the hook for lazy loading, say a folder of devices
read from an inventory API, but the function itself must stay quick. It runs
on the main thread in the middle of GTK's own work; a network call in it
freezes the window.

What you return is owned by the tree model ("transfer full"). gtklab builds
a new store on every call. The lab keeps one store per group and returns a
new reference to it. Both are correct. Returning a pointer without a
reference of its own is not: the tree model will release a reference that
was never its to release.

### Filtering a tree

Filtering a flat list is one decision. Filtering a tree is three, because
you can cut at three places:

| Where you filter | What it can express | What it costs |
|---|---|---|
| `GtkFilterListModel` over the flattened rows | only rows that exist; a session in a closed folder is not a row, so it cannot match | a folder whose name does not match hides its children |
| `GtkFilterListModel` over the root | folders only | sessions are not filtered at all |
| In the core, then rebuild the root (gtklab) | anything the core can test: name, host or user, folders without a match dropped | a rebuild per change, and the open folders to restore |

gtklab takes the third. `filter_sessions` already exists in Chapter 4's
core, with tests, so the GTK side only has to replace the root model with
its result. The price is that new folder items start closed, which is the
problem the rest of the chapter solves.

### Keeping the user's place

The tree has two kinds of state. The sessions come from the file. Which
folders are open belongs to the user, and nothing in the file records it. A
rebuild throws away every `GtkTreeListRow`, and their expanded flags with
them, so the window has to carry that state across by name.

![The search cycle. The first character saves the user's open folders; while searching, every match is open; clearing the search re-opens the saved ones.](diagrams/ch06-search.svg)

The design is a small state machine. Browsing, the user's folders are open.
The first keystroke of a search records them by name. While searching, every
folder with a match is open, because a hidden match is useless. Clearing the
search rebuilds the full tree and re-opens the recorded names. Chapter 7's
Reload and Checkpoint B reuse the same capture and re-expand steps.

Re-opening is itself an expansion, so the first diagram happens again
underneath the code that walks the rows: each folder it opens inserts rows
after it, while the walk is still going.

## Build

The chain gains a link. `passthrough` is `FALSE`, so the list items are
`GtkTreeListRow`s wrapping your items; `autoexpand` is `FALSE`, so folders
start closed:

```{.cpp include="ch06-tree/src/app_window.cpp" from="root_ = g_list_store_new" to="gtk_single_selection_new(G_LIST_MODEL(g_object_ref(tree_model_)));"}
```

The child function returns a new store of the folder's sessions, or `NULL`
for a session:

```{.cpp include="ch06-tree/src/app_window.cpp" fn="GListModel *AppWindow::create_child_model("}
```

The row widget is Chapter 5's row inside an expander. In `bind`, the item is
fetched from the tree row; `gtk_tree_list_row_get_item` returns a new
reference, so it is released at the end:

```{.cpp include="ch06-tree/src/app_window.cpp" fn="void AppWindow::on_factory_bind("}
```

### Rebuilding without losing the user's place

Rebuilding the root replaces every folder item, and new items start
collapsed. So the window remembers which folders were open before it
rebuilds, and opens them again afterwards. Remembering is a walk over the
flattened rows:

```{.cpp include="ch06-tree/src/app_window.cpp" fn="std::set<std::string> AppWindow::capture_expanded("}
```

The rebuild swaps all the folders in with one `g_list_store_splice`, which
sends the view a single change instead of one per folder, then re-expands.
Expanding a row inserts rows after it, so the loop re-reads the count each
time around:

```{.cpp include="ch06-tree/src/app_window.cpp" fn="void AppWindow::rebuild_tree("}
```

The search handler decides what "open" means. When a search starts, it
captures the user's tree; while searching, every folder with a match is open;
when the search is cleared, the captured tree comes back:

```{.cpp include="ch06-tree/src/app_window.cpp" fn="void AppWindow::on_search_changed("}
```

Activating a row (Enter, or a double-click) toggles a folder or opens a
session, which for now prints its command line:

```{.cpp include="ch06-tree/src/app_window.cpp" fn="void AppWindow::on_row_activated("}
```

## Done when

Typing filters the tree, clearing the search restores exactly the folders that
were open, and Enter on a session prints its command. In the recorded run,
activating `core1-den` printed `open core1-den: ssh admin@10.0.0.1`.

![Left to right: folders closed; Core opened; filtered to "fra", with every match shown; filter cleared, with Core open again as the user left it.](images/ch06-tree.png)

::: {.box title="Debugger drill"}
Set a breakpoint in `create_child_model`, or watch its `g_debug` line in the
scratch config, and predict when it will run before you start. The usual
guess is "the first time a folder is opened". The measured answer is: for
every visible folder at startup, because the expander must know whether a
row has children before it can decide to draw a triangle; and again for
several folders on each expand and collapse. So the function must be cheap,
and every call must hand over a reference the tree can own: gtklab builds a
new store each time; lab06 returns a new reference to a store it keeps.
:::

::: {.box title="Break it"}
Pass `TRUE` for `passthrough`. The list items are now your
`GtklabSessionItem`s, not tree rows, and `bind` casts them to
`GtkTreeListRow`. Under **stop on GTK criticals** the program stops at
startup with `invalid cast from 'GtklabSessionItem' to 'GtkTreeListRow'`.
Passthrough mode exists for views that do not need expanders; with it, the
tree list model hands your items straight through.
:::

## Common mistakes

**Caching the row count while expanding.** Every expansion inserts rows. A
loop over a count read once before it starts misses rows or reads past the
end.

**Returning a model you do not own.** The tree list model takes ownership of
what you return and drops it when it is done. Return a new model, or
`g_object_ref` one you keep; a borrowed pointer is freed out from under you.

**Filtering the flattened rows.** It hides children of folders whose names do
not match. Filter the data, then rebuild.

**Forgetting the unref after `gtk_tree_list_row_get_item`.** It returns a new
reference.

::: {.box title="Read in tmssh"}
`build_sidebar`, `create_child_model`, `rebuild_tree` and `capture_expanded`.
tmssh's tree is driven by the same capture, rebuild, re-expand cycle.
:::

## Review

1. A collapsed folder is not in the flat list at all, yet still has its
   sessions. Where do they live while it is closed?
2. What does the factory receive as its list item when the model is a
   `GtkTreeListModel` with `passthrough` set to `FALSE`, and how do you get
   from it to your session?
3. The lab's child function ran for both folders before anything was
   clicked. Why, and what does that mean for what the function may do?
4. Why must `unbind` call `gtk_tree_expander_set_list_row` with `nullptr`?
   What would a recycled row show otherwise?
5. Why does gtklab filter in the core and rebuild, instead of putting a
   `GtkFilterListModel` over the tree list model?
6. After a rebuild, every folder is closed. Why, and why does gtklab
   remember open folders by name rather than by row position?
7. `rebuild_tree` re-reads `g_list_model_get_n_items` on every pass of its
   loop. Describe a sessions file for which a count read once would skip a
   folder that should be re-opened.
8. While a search is active, the user closes a folder, then clears the
   search. Which folders are open afterwards, and why?

**Questions for deeper thought.** A network inventory tool shows sites,
devices and interfaces, and the interfaces come from a slow API. What would
your child function return for a device, and when would it fill it? If the
same device can appear under two sites, what should identify a row across a
rebuild? Should the search state survive a restart, and where would it be
stored?

## Exercises

1. **Keep the selection.** In `rebuild_tree` in `ch06-tree`, note the
   selected session's name before the splice and select it again afterwards
   with `gtk_single_selection_set_selected`, finding its new position by
   walking the rows as `capture_expanded` does. Done when selecting
   `core1-den` and typing `core` leaves `core1-den` selected.
2. **Matches out of total.** While a filter is active, make a folder's
   detail line read `1 of 2 sessions` instead of `1 session`. The filtered
   `Folder` only holds the matches, so look the total up in `folders_` by
   name. Done when the counts are right while filtering and unchanged
   without a filter.
3. **Expand all, collapse all.** Add two functions to `AppWindow` that open
   or close every folder row, modelled on the loop in `rebuild_tree`, and
   call them from a temporary button. Done when both work with some folders
   already open, and you can say why collapsing needs no special care about
   the count.
4. **Show the match.** In `on_factory_bind`, when `filter_` is not empty,
   set the session's name with `gtk_label_set_markup` and wrap the matching
   part in `<b>`. The factory callbacks already receive the window as
   `user_data`; use it. Escape the name with `g_markup_escape_text` first.
   Done when typing `fra` shows `fra` in bold in `edge1-fra1`, and a session
   named `a<b` does not break the label.

## Isolated lab

In gtklab, the tree is tangled up with filtering, re-expansion and a
session model. The lab keeps only the tree: two groups of `GtkStringObject`
leaves, each group carrying its child store as object data, a
`GtkTreeListModel` with `passthrough` and `autoexpand` both `FALSE`, and a
factory whose rows are an expander around a label. Every callback prints,
including the child function, so the question "when does it run?" is
answered by reading the terminal.

```{.cpp include="labs/lab06-tree/src/main.cpp"}
```

Build and run from `labs/lab06-tree`:

```
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/lab
```

In the recorded run, `children_for: Production -> model` and
`children_for: Laboratory -> model` ran at startup, before any click, each
with a `setup` and a `bind`. Clicking Production's expander ran
`children_for: Production -> model` again, then a `setup` and `bind` for
Router A and Switch B, each preceded by `children_for: ... -> leaf`.

| Stop at | What it shows |
|---|---|
| `children_for` at startup | the stack: `gtk_tree_list_row_is_expandable`, called by `gtk_tree_expander_set_list_row` in your own `bind_row`; the expander asking, not a click |
| `children_for` after a click | the expand itself; the store returned is the same one, with one more reference |
| `bind_row` | `gtk_list_item_get_item` is a `GtkTreeListRow`; `depth` is 0 for groups and 1 for leaves |
| `unbind_row` | the expander letting go of its row before the widget is reused |

Experiments:

1. **Open everything.** Pass `TRUE` for `autoexpand`. In the recorded run,
   `children_for` ran for all six items, groups and leaves, before the first
   `setup`, and the window opened with both groups expanded.
2. **Leave out the row.** Delete the `gtk_tree_expander_set_list_row` call
   in `bind_row`. The labels appear without triangles or indent, clicking
   does nothing, and `children_for` never ran at all: nothing asked.
3. **Passthrough.** Pass `TRUE` for `passthrough` and run **lab: stop on
   GTK criticals**. It stopped at the first bind with
   `invalid cast from 'GtkStringObject' to 'GtkTreeListRow'`: the list
   items are now your strings.
4. **A third level.** In `activate`, after the groups are built, add a group
   "Rack 1" with "Server E" and "Server F" to Production's child store with
   `add_group`. Expanding Production then showed Rack 1 at depth 1 with its
   own triangle, and printed `children_for: Rack 1 -> model`. Nothing in
   the factory or the view changed.

![Lab 6 with Production expanded.](images/lab06-tree.png){width=60%}
