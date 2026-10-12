# Chapter 2 — GObject: references, signals, ownership

GTK is written in C, and its widgets are objects in GObject, a small object
system written in C. You do not need to know much of GObject to use GTK, but
you do need to know three things well: how objects are kept alive, how
signals call your code, and how to attach your own data to an object so it
dies when the object does. Almost every hard bug in a GTK program is one of
those three going wrong, and this chapter builds a tiny program that shows
all three working, and then breaks them on purpose.

## The idea

### Reference counting, and the floating reference

Every GObject has a reference count. `g_object_ref` adds one,
`g_object_unref` removes one, and when the count reaches zero the object is
destroyed. That is the whole rule, and it is the rule for application
objects, models and settings objects.

Widgets add one twist. A new widget starts with a **floating** reference: a
reference that nobody owns yet. The first container you add it to *sinks*
that reference and becomes the owner. So this code has no leak and no
unref, because the box owns the button and the window owns the box:

```cpp
GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
GtkWidget *button = gtk_button_new_with_label("Go");
gtk_box_append(GTK_BOX(box), button);    // box sinks the button's reference
gtk_window_set_child(GTK_WINDOW(win), box);
```

The consequence is the part that matters: **removing a widget from its
parent destroys it**, unless you took a reference of your own first. A
pointer you kept to it is then a pointer to freed memory.

![Who owns what in this chapter's program. Solid arrows own a reference; the dashed arrow is a signal connection that is broken automatically when the counter button is destroyed.](diagrams/ownership.svg)

### Signals

A signal is a named event on an object: `clicked` on a button,
`close-request` on a window. You connect a C function to it with
`g_signal_connect(object, "clicked", G_CALLBACK(handler), user_data)`, and
when the object emits the signal GObject calls your function with the object,
the signal's own arguments, and your `user_data` pointer.

That `user_data` pointer is where lifetime bugs come from. GObject stores it
and hands it back; it does not know what it points at and does not keep it
alive. If it points at something that is destroyed before the signal fires,
your handler gets a dangling pointer.

`g_signal_connect_object` is the fix for the most common case, where
`user_data` is itself a GObject. It ties the connection to that object's
lifetime: when the object is destroyed, the handler is disconnected, so it
can never be called with a dead pointer.

### Data attached to objects

`g_object_set_data_full(object, "key", pointer, destroy)` attaches your own
data to any GObject, with a function to call when the object is destroyed (or
the key is replaced). It is how a widget can own a C++ object: attach it,
and give `delete` as the destroy function. You will use exactly this in
Chapter 8 to make a notebook page own everything behind its tab.

## Build

A counter button that owns a C++ `Counter` struct, and a second button that
removes the counter from the window. Replace `main.cpp` with:

```{.cpp include="ch02-gobject/src/main.cpp"}
```

Read the two connections side by side. The counter button's handler gets no
`user_data`, because it finds its `Counter` on the button itself with
`g_object_get_data`. The remove button's handler gets the counter button as
`user_data`, and it is connected with `g_signal_connect_object`, so the
connection dies with the counter button.

The lambda passed to `g_object_set_data_full` is a capture-less lambda,
which converts to a plain function pointer; that is the form a C callback
needs. You will see this pattern throughout the book.

## Done when

Clicking the counter counts, Remove prints `Counter freed after N clicks`,
and clicking Remove a second time does nothing.

![Three clicks, then Remove: the counter button is gone and the `Counter` it owned has been deleted.](images/ch02-counter.png)

::: {.box title="Debugger drill"}
Set a breakpoint in `~Counter` and click Remove. The recorded stack, from the
top:

```
#0  Counter::~Counter at ../src/main.cpp:6
#1  operator() at ../src/main.cpp:47
#3  g_datalist_clear at libglib-2.0.so.0
#4  g_object_unref at libgobject-2.0.so.0
 ... GTK and signal emission frames ...
```

Further down are your `on_remove_clicked` and the button's `clicked` signal.
Read it upward: the click ran your handler, your handler removed the button
from the box, the box dropped the last reference, GObject destroyed the
button, and destroying it cleared its data, which called your lambda, which
deleted the `Counter`. You never wrote `delete` for it anywhere else.
:::

::: {.box title="Break it"}
1. Append `count` to the box a second time. Run the **stop on GTK criticals**
   configuration. It stops on
   `gtk_box_append: assertion 'gtk_widget_get_parent (child) == NULL' failed`,
   with your line on the stack. A widget has one parent.
2. Change `g_signal_connect_object(...)` to
   `g_signal_connect(remove, "clicked", G_CALLBACK(on_remove_clicked), count)`
   and click Remove twice. The second click hands the destroyed button to
   your handler, and GLib prints
   `invalid unclassed pointer in cast to 'GtkWidget'`. Then build with
   AddressSanitizer (Chapter 16 shows how) and try again: ASan says nothing.
   The freed memory belonged to GLib, which is not built with ASan, so the
   only evidence is GTK's own type check. Chapter 16 comes back to the two
   kinds of lifetime bug and the tool for each.
:::

## Common mistakes

**Keeping a pointer to a child you removed.** Removing a widget from its
parent destroys it. Take a reference with `g_object_ref` first if you need it
afterwards, and drop it with `g_object_unref` when you are done.

**Unreferencing a widget you never referenced.** The container owns it. An
extra `g_object_unref` destroys it while it is still in the window, and the
crash comes later, somewhere else.

**Passing `this` as `user_data` and forgetting it.** Every handler connected
with a raw pointer must be disconnected before that pointer dies. Chapter 16
is about the time this goes wrong at quit.

::: {.box title="Read in tmssh"}
`add_tab` in `src/app_window.cpp`: each notebook page owns its `Tab` through
`g_object_set_data_full`, exactly like the `Counter` here, except that
deleting a `Tab` closes a pty and a log file.
:::

::: {.box title="Check yourself"}
Who owns a widget after `gtk_box_append`? When does the destroy function
given to `g_object_set_data_full` run? What does `g_signal_connect_object`
protect against that `g_signal_connect` does not?
:::
