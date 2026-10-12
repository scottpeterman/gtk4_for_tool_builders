# Chapter 5 — List views I: models and factories

The sidebar finally shows sessions. This chapter puts every session from
Chapter 4's model into a `GtkListView`, which means learning the list widget
design that replaced GTK 3's `GtkTreeView`, and writing your first GObject
class.

::: {.box title="Recall"}
Which layer owns the session data now (Chapter 4)? What does the floating
reference on a new widget mean (Chapter 2)?
:::

::: {.box title="In this chapter"}
- Name the four pieces of a GTK 4 list (model, selection, view, factory) and
  say which one to look at for a given bug.
- Write a factory that builds row widgets once and refills them as they are
  recycled.
- Follow every reference in the model chain, and know which constructors
  take ownership of their argument.
- Define a final GObject type that carries plain C++ data into a
  `GListModel`.
:::

## Concepts

### Why a list needs a model

The obvious first sidebar is a box with one label per session. It works for
a dozen sessions. Then a session is renamed, the list is filtered, a row is
selected, and the question becomes: which copy is the truth? The label's
text, the vector in `AppWindow`, or the file on disk? Every feature added to
a widget-per-item list has to answer that again.

GTK 4's list widgets answer it once. The data lives in a model. The view
shows whatever the model holds and never stores anything of its own. When
the model changes, it emits `items-changed` with a position and the number
of items removed and added, and that signal is the only way the view learns
anything. Chapter 4 already put the sessions in a GTK-free core; the model
here is a thin adapter over it, not a second copy of the truth.

### Four objects, one job each

GTK 4's list widgets separate four things that GTK 3 tangled together.

The **model** holds the data. It is any object implementing the
`GListModel` interface: a list of GObjects that can tell you how many items
it has, give you one by position, and announce when items change. GLib's
`GListStore` is the general-purpose implementation.

The **selection model** wraps the data model and adds which items are
selected. `GtkSingleSelection`, `GtkMultiSelection` and `GtkNoSelection` are
the usual choices. It is an adapter over the same items, not a copy of them.

The **view**, `GtkListView`, shows the selection model's items. It never
creates a widget per item. It asks a **factory** to create a few row widgets,
enough to fill the screen and a margin around it, and then reuses them as
rows scroll in and out, refilling each one for whichever item it now shows.
That is why a list of a million items is as cheap as a list of a hundred.

![The model chain. Data flows right through the models; the view asks the factory for row widgets and binds them to items as they become visible.](diagrams/list-model-chain.svg)

Because each piece has one job, a list bug usually lives in one place.
A missing session is a model problem: count the items after `fill_list`.
The right number of rows with the wrong text is a `bind` problem. Old text
flashing while you scroll is an `unbind` problem. The wrong row highlighted
is the selection model. Start at the left of the chain and stop at the first
piece that is wrong.

### Recycling: a row is not an item

`GtkSignalListItemFactory` turns the factory's work into four signals:

- **`setup`**: create the widgets for one row. No item yet.
- **`bind`**: fill an existing row's widgets for an item.
- **`unbind`**: the row is about to show a different item, or none.
- **`teardown`**: the row's widgets are being destroyed.

![One row widget over time. It is built once, then bound and unbound for different items as the list scrolls.](diagrams/ch05-row-life.svg)

The discipline that makes recycling work is simple: build widgets in `setup`,
only fill them in `bind`, and in `unbind` let go of anything that refers to
the old item. Labels are easy, because the next `bind` overwrites them. A
signal connected to the item, a timer, or a pointer cached on the row are
not: left behind, they outlive the item's turn in that row and act on the
wrong session.

Do not count on `bind` running once per item, either. It runs whenever a
row needs refilling, and the isolated lab shows a selection change
re-binding two rows whose items had not changed at all. `bind` must be cheap
and must produce the same row every time for the same item.

### GtkListBox or GtkListView

GTK 4 still has `GtkListBox`, and it is the right tool for some lists.

| | `GtkListBox` | `GtkListView` |
|---|---|---|
| Row widgets | one per item, made by `create_widget_func` when the item is added | a few, recycled by a factory |
| Cost of 2,000 items | 2,000 rows of widgets | about as many rows as fit on screen, plus a margin |
| Rows can differ | freely: each row is its own widget | every row comes from one factory, so `bind` must handle each kind |
| Good for | settings pages, short lists, rows with their own controls | sessions, logs, files, anything that can grow |

A session manager's list grows with the user's network, so gtklab uses
`GtkListView` from the start.

### References along the chain

Every arrow in the model chain is a reference, and the constructors that
build the chain take ownership of what you pass them. GTK's documentation
marks those parameters "transfer full": the callee now owns the reference
you handed over, and you must not release it again. If you want to keep
using the object, as `AppWindow` keeps `store_`, take a second reference for
yourself before handing one over.

![Who holds a reference to what. The window and the selection each hold their own reference to the store; the store holds one to every item.](diagrams/ch05-refs.svg)

`g_object_unref` does not mean delete. It gives back one reference; the
object is finalized only when the last one goes. That is why appending an
item to the store and then unreffing it is correct: the store holds its own
reference, and the item lives as long as the store keeps it.

### Defining a GObject type

Items in a `GListModel` must be GObjects, and a session is a plain C++
struct. Chapter 4 kept it that way on purpose, so the core could be tested
without GTK. It needs a small GObject wrapper at the GTK boundary, which
means defining a GObject type of your own. It takes two macros and three
functions.

In the header, `G_DECLARE_FINAL_TYPE(GtklabSessionItem, gtklab_session_item,
GTKLAB, SESSION_ITEM, GObject)` declares the type: a `GtklabSessionItem`
struct, a `GTKLAB_SESSION_ITEM()` cast macro, a `GTKLAB_IS_SESSION_ITEM()`
check, and `gtklab_session_item_get_type()`. "Final" means nothing can
subclass it, which is what you want for almost every application type.

In the source file, you define the instance struct, whose first member must
be the parent type's instance (`GObject parent_instance`), and then
`G_DEFINE_FINAL_TYPE(GtklabSessionItem, gtklab_session_item, G_TYPE_OBJECT)`
registers the type. That macro expects you to write:

- **`class_init`**, run once per type, which fills in virtual functions such
  as `finalize`;
- **`init`**, run for every new instance, like a constructor;
- and, if the type holds anything that needs freeing, a **`finalize`** that
  frees it and then chains up to the parent's `finalize`.

There is one trap for C++. GObject allocates and zero-fills instances itself;
it never runs C++ constructors or destructors. So a `std::string` member must
be constructed by hand in `init`, with placement `new`, and destroyed by hand
in `finalize`. Forget the first and you use an object that was never
constructed; forget the second and you leak.

## Build

### The item type

```{.cpp include="ch05-listview/src/session_item.h"}
```

```{.cpp include="ch05-listview/src/session_item.cpp"}
```

### AppWindow

From this chapter, the window and everything it owns live in one class:

```{.cpp include="ch05-listview/src/app_window.h"}
```

The window owns its `AppWindow` the same way Chapter 2's button owned its
`Counter`: through `g_object_set_data_full`. Chapter 16 changes how that
ownership is wired, and the reason is one of the most useful lessons in the
book.

```{.cpp include="ch05-listview/src/app_window.cpp" fn="AppWindow::AppWindow("}
```

`main.cpp` now only creates it:

```{.cpp include="ch05-listview/src/main.cpp" fn="static void on_activate("}
```

### The model chain

```{.cpp include="ch05-listview/src/app_window.cpp" fn="GtkWidget *AppWindow::build_sidebar("}
```

Follow the references. `g_list_store_new` returns a store with one reference,
which `store_` keeps. `gtk_single_selection_new` *takes* a reference to the
model it wraps, so the window passes it a new one with `g_object_ref`.
`gtk_list_view_new` takes ownership of both the selection model and the
factory, so neither needs an unref. GTK's documentation marks every such
parameter "transfer full"; it is worth reading for every function that takes
a model.

Filling the store is a loop of append and unref, because the store keeps its
own reference to each item:

```{.cpp include="ch05-listview/src/app_window.cpp" fn="void AppWindow::fill_list("}
```

### The factory

`setup` builds the row and stores non-owning pointers to its parts on the row
widget, so `bind` can find them without walking the widget tree:

```{.cpp include="ch05-listview/src/app_window.cpp" fn="void AppWindow::on_factory_setup("}
```

`bind` fills those widgets for one item:

```{.cpp include="ch05-listview/src/app_window.cpp" fn="void AppWindow::on_factory_bind("}
```

`unbind` clears them, so a row never shows a stale name for a moment while it
is being reused:

```{.cpp include="ch05-listview/src/app_window.cpp" fn="void AppWindow::on_factory_unbind("}
```

## Done when

Two thousand generated sessions scroll smoothly. The repository's
`tools/gen_sessions.py` writes them:

```
tools/gen_sessions.py 2000 ~/.config/gtklab/sessions.ini
```

![2,000 sessions in a `GtkListView`. Each row is a recycled widget, re-bound as it scrolls into view.](images/ch05-listview.png){width=75%}

::: {.box title="Debugger drill"}
Run the **scratch config** launch configuration with 2,000 sessions and count
the `bind` lines it prints. In the recorded run there were about 200 at
startup, not 2,000: GTK binds the visible rows and a margin around them.
After thirty notches of the mouse wheel the total was 376. Those extra binds
are the same few dozen row widgets being re-bound to new items. Then set a
breakpoint in `on_factory_bind`, scroll one notch, and watch it run once per
row that came into view.
:::

::: {.box title="Break it"}
Remove `gtk_widget_set_vexpand(scroller, TRUE)` from `build_sidebar`. With
2,000 sessions, the list shrinks to a single visible row. A scrolled window's
natural height is tiny, and nothing asked for more. Open the Inspector,
select the `GtkScrolledWindow` and set `vexpand` back on: the list grows to
fill the sidebar.
:::

![The Break it: without `vexpand`, one row of 2,000.](images/ch05-breakit-vexpand.png){width=40%}

## Common mistakes

**Icons that show as a broken image.** The symbolic icons here are SVG files,
and GTK loads SVG through a gdk-pixbuf loader in a package of its own. On a
minimal system without it (`librsvg2-common` on Debian and Ubuntu), every
icon renders as a broken-image placeholder. The screenshots for this book
showed exactly that until the package was installed, which is also why
tmssh's `scripts/build.sh` installs it.

**Building widgets in `bind`.** It works, slowly, and leaks a widget every
time a row is reused. Build in `setup`, fill in `bind`.

**Forgetting the unref after `g_list_store_append`.** The store takes its own
reference; yours leaks.

**Handing the store to the selection model without a reference.** The
selection takes ownership. If the window also unrefs `store_` in its
destructor, the store is freed twice.

**C++ members without placement new.** GObject never runs constructors. A
`std::string` in the instance struct that is used before being constructed
is undefined behaviour that usually looks fine until it does not.

::: {.box title="Read in tmssh"}
`on_factory_setup`, `on_factory_bind` and `on_factory_unbind` in
`src/app_window.cpp`. tmssh's rows show a folder or a session with an icon, a
name and a count; the structure is the same as gtklab's.
:::

## Review

1. A row shows `core2-den` with `core1-den`'s address underneath. Which of
   the four pieces do you look at first, and why not the model?
2. What can `setup` do that `bind` must not, and what goes wrong if you
   build the row's widgets in `bind` instead?
3. `unbind` in gtklab only clears two labels. Give an example of something a
   row might hold that would make an empty `unbind` a real bug, not a
   cosmetic one.
4. Why must items in a `GListModel` be GObjects, and why does gtklab wrap
   `Session` instead of making it a GObject?
5. `build_sidebar` calls `g_object_ref(store_)` before
   `gtk_single_selection_new`. What happens without it, and when?
6. `fill_list` unrefs each item right after appending it. Why is the item
   still alive afterwards?
7. In `gtklab_session_item_init`, what would happen if the placement `new`
   for `session` were removed, and why does the compiler not stop you?
8. When would you choose `GtkListBox` over `GtkListView` in a tool you are
   writing?

**Questions for deeper thought.** A log viewer appends a few hundred lines
a second to a `GListStore`. What does each append cost the view, and how
would you batch them? Your sessions file grows a field the sidebar does not
show. Should the GObject wrapper copy it, or hold a pointer back into the
core's data, and what does each choice cost when the file is reloaded?

## Exercises

1. **Show the selection.** In `build_sidebar` in `ch05-listview`, connect
   `notify::selected` on the `GtkSingleSelection` and, in the handler, put
   the selected session's name in the "No tabs open" label
   (`gtk_single_selection_get_selected_item` returns the item without a new
   reference). Done when clicking a row changes the label, and you can say
   why no session was copied to do it.
2. **Folder in the detail line.** Change `on_factory_bind` so remote
   sessions show `Core · admin@10.0.0.1`, using `Session::folder`. Done when
   every row shows its folder, and `unbind` still leaves no stale text.
3. **Count the rows.** Add counters for `setup` and `bind` calls (static
   variables are fine in an exercise) and print them in `on_close_request`.
   Done when, with 2,000 sessions and some scrolling, you can state how many
   row widgets existed and how many times they were bound.
4. **One change instead of many.** Rewrite `fill_list` to build a
   `std::vector<gpointer>` of items and insert them with one
   `g_list_store_splice`. Connect `items-changed` on `store_` and print its
   arguments. Done when the old loop prints one line per session and the new
   one prints a single line.
5. **Watch teardown.** Connect a `teardown` handler in `build_sidebar` that
   prints a line. Done when you have seen when it runs, and can explain why
   gtklab does not need one.

## Isolated lab

gtklab's list comes with a session parser, a GObject subclass and a window
full of other widgets. The lab strips all of that away to show the list
pipeline alone: plain `GObject`s with a name attached by
`g_object_set_data_full`, a `GListStore`, a `GtkSingleSelection`, a
`GtkListView` and a factory whose `bind` and `unbind` print what they do. An
"Add session" button appends one item per click. With nothing else on the
screen, every line in the terminal is the list talking.

```{.cpp include="labs/lab05-listmodel/src/main.cpp"}
```

Build and run from `labs/lab05-listmodel`:

```
meson setup build-debug --buildtype=debug
meson compile -C build-debug
./build-debug/lab
```

The launch configurations are the same as every lab's: **lab**, **lab:
stop on GTK criticals**, **lab + GTK Inspector**, **lab (gdb)** and **lab
(ASan)**. In the recorded run, three clicks printed `bind: Session 1` to
`bind: Session 3`, and quitting printed three `unbind` lines.

| Stop at | What it shows |
|---|---|
| `add_item` | the click arriving; step over `g_list_store_append` and the model has one more item before any row exists for it |
| `bind` | `gtk_list_item_get_item` returning the object that `add_item` created; the label is the one `setup` made |
| `unbind` | a row giving up its item; at quit, one stop per bound row |
| `g_list_store_append` | the stack: GLib at the top, your `add_item` below it, the button's `clicked` emission below that |

Experiments:

1. **Many items, few rows.** Make `add_item` append 1,000 items per click
   in a loop. In the recorded run one click produced 205 `bind` lines, not
   1,000; thirty wheel notches added 141 more, each one after an `unbind`;
   quitting printed 206 `unbind` lines. The view never bound more than about
   two hundred rows.
2. **bind is not once per item.** Print `gtk_list_item_get_position(item)`
   in `bind`, add three sessions, then click Session 2. In the recorded run
   the click printed `unbind`, `bind: Session 1 at 0`, `unbind`,
   `bind: Session 2 at 1`: the selection moved, and both rows were bound
   again for items that had not changed.
3. **Take the selection away.** Replace `gtk_single_selection_new` with
   `gtk_no_selection_new`. The rows still appear, the first row is no
   longer highlighted at startup, and clicking a row printed nothing at all.
   Only one responsibility changed.
4. **Follow a reference.** Remove `g_object_unref(object)` from `add_item`,
   build `build-asan`, and run `ASAN_OPTIONS=detect_leaks=1 ./build-asan/lab`
   from a terminal (the launch configuration turns leak detection off). Add
   three sessions and quit. The report is long either way, because GTK and
   its libraries keep some memory until exit; search it for `add_item`. In
   the recorded run the unchanged lab had no `add_item` frame in its report,
   and the changed one had leak stacks through `add_item`, starting at the
   `g_object_new` line. The store gave its reference back; yours was never
   given back.

![Lab 5 after three clicks on Add session.](images/lab05-listmodel.png){width=55%}
