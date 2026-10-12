# Chapter 5 — List views I: models and factories

The sidebar finally shows sessions. This chapter puts every session from
Chapter 4's model into a `GtkListView`, which means learning the list widget
design that replaced GTK 3's `GtkTreeView`, and writing your first GObject
class.

::: {.box title="Recall"}
Which layer owns the session data now (Chapter 4)? What does the floating
reference on a new widget mean (Chapter 2)?
:::

## The idea: models, factories and recycling

GTK 4's list widgets separate three things that GTK 3 tangled together.

The **model** holds the data. It is any object implementing the
`GListModel` interface: a list of GObjects that can tell you how many items
it has, give you one by position, and announce when items change. GLib's
`GListStore` is the general-purpose implementation.

The **selection model** wraps the data model and adds which items are
selected. `GtkSingleSelection`, `GtkMultiSelection` and `GtkNoSelection` are
the usual choices.

The **view**, `GtkListView`, shows the selection model's items. It never
creates a widget per item. It asks a **factory** to create a few row widgets,
enough to fill the screen and a margin around it, and then reuses them as
rows scroll in and out, refilling each one for whichever item it now shows.
That is why a list of a million items is as cheap as a list of a hundred.

![The model chain. Data flows right through the models; the view asks the factory for row widgets and binds them to items as they become visible.](diagrams/list-model-chain.svg)

`GtkSignalListItemFactory` turns that into four signals:

- **`setup`**: create the widgets for one row. No item yet.
- **`bind`**: fill an existing row's widgets for an item.
- **`unbind`**: the row is about to show a different item, or none.
- **`teardown`**: the row's widgets are being destroyed.

The discipline that makes recycling work is simple: build widgets in `setup`,
only fill them in `bind`, and in `unbind` let go of anything that refers to
the old item.

## The idea: defining a GObject type

Items in a `GListModel` must be GObjects, and a session is a plain C++
struct. So it needs a small GObject wrapper, which means defining a GObject
type of your own. It takes two macros and three functions.

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

::: {.box title="Check yourself"}
What does `setup` do that `bind` must not? Why must list items be GObjects?
Which of the model chain's constructors take ownership of their argument?
:::
