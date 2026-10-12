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

## The idea: a tree is a list that grows

GTK 4 has no tree widget. It has `GtkTreeListModel`, a model that turns a
tree into a flat list. You give it a root model, here the folders, and a
function that returns a model of children for an item, or `NULL` for an item
that has none. When a row is expanded, its children are inserted into the
flat list after it; when it is collapsed, they are removed. The list view
underneath neither knows nor cares that it is showing a tree.

Each item in the flattened list is a `GtkTreeListRow`, which wraps your item
and knows its depth and whether it is expanded. In the row widget, a
`GtkTreeExpander` draws the disclosure triangle and indents the row by its
depth; you hand it the row in `bind`.

![The model chain for a tree. The tree list model calls `create_child_model` for folder rows, and inserts the children after the folder when it is expanded.](diagrams/tree-model-chain.svg)

Filtering could be done with `GtkFilterListModel`, but a filter applied to a
flattened tree filters rows, not sessions: a folder whose name does not
match would hide all its children. It is simpler, and easier to test, to
filter in the core (Chapter 4 already has `filter_sessions`) and rebuild the
root model from the result.

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
and it must return a new model every time it is called.
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

**Returning the same child model twice.** The tree list model owns what you
return. Return a new model on every call.

**Filtering the flattened rows.** It hides children of folders whose names do
not match. Filter the data, then rebuild.

**Forgetting the unref after `gtk_tree_list_row_get_item`.** It returns a new
reference.

::: {.box title="Read in tmssh"}
`build_sidebar`, `create_child_model`, `rebuild_tree` and `capture_expanded`.
tmssh's tree is driven by the same capture, rebuild, re-expand cycle.
:::

::: {.box title="Check yourself"}
When does `create_child_model` run? What is a `GtkTreeListRow`, and where
does your item live inside it? What survives a rebuild of the tree, and how?
:::
