# Chapter 2 — GObject: references, signals, ownership

GTK is written in C, and its widgets are objects in GObject, a small object
system written in C. You do not need to know much of GObject to use GTK, but
you do need to know three things well: how objects are kept alive, how
signals call your code, and how to attach your own data to an object so it
dies when the object does. Almost every hard bug in a GTK program is one of
those three going wrong, and this chapter builds a tiny program that shows
all three working, and then breaks them on purpose.

::: {.box title="In this chapter"}
- Tell an owning reference from a borrowed pointer, and say who owns each
  widget in a window.
- Predict when a widget is destroyed, and what that does to pointers you
  kept.
- Connect a signal handler that cannot be called with a dead pointer.
- Make a widget own a C++ object, so the object is deleted when the widget
  is.
:::

## Concepts

### An object system written in C

GTK's API looks like a collection of C functions, but behind it is a
runtime type system. Most of what you handle derives from `GObject`, which
supplies four things C does not have: a runtime type, reference-counted
lifetime, signals, and named properties. Every widget is a GObject; not
every GObject is a widget. The type chains for this chapter's objects, as
GLib reports them:

| Object | Type chain |
|---|---|
| a button | `GtkButton` → `GtkWidget` → `GInitiallyUnowned` → `GObject` |
| a window | `GtkWindow` → `GtkWidget` → `GInitiallyUnowned` → `GObject` |
| the application | `GtkApplication` → `GApplication` → `GObject` |

The compiler knows none of this. A cast macro such as `GTK_BOX(widget)` or
`G_OBJECT(button)` is a checked cast at run time: it creates nothing and
takes no reference, and if the object is not of that type GLib prints a
critical warning naming the type it expected. That check is often the only
evidence of a lifetime bug, as the second Break it shows.

Properties are the fourth piece. A property is a named, typed value on an
object (a paned's `position`, a stack's `visible-child-name`) that can be
read and set by name with `g_object_get` and `g_object_set`. The GTK
Inspector is built on them, which is why Chapter 3's drill can change a
paned's position by typing a number.

### Reference counting, and the floating reference

Every GObject has a reference count. `g_object_ref` adds one,
`g_object_unref` removes one, and when the count reaches zero the object is
destroyed. That is the whole rule, and it is the rule for application
objects, models and settings objects.

Widgets add one twist, which is what `GInitiallyUnowned` in the table means.
A new widget starts with a **floating** reference: a reference that nobody
owns yet. The first container you add it to *sinks* that reference and
becomes the owner. So this code has no leak and no unref, because the box
owns the button and the window owns the box:

```cpp
GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
GtkWidget *button = gtk_button_new_with_label("Go");
gtk_box_append(GTK_BOX(box), button);    // box sinks the button's reference
gtk_window_set_child(GTK_WINDOW(win), box);
```

The consequence is the part that matters: **removing a widget from its
parent destroys it**, unless you took a reference of your own first. A
pointer you kept to it is then a pointer to freed memory.

Windows are the exception to the parent rule, because they have no parent.
A new window is not floating: GTK keeps it in its list of toplevel windows,
and closing the window is what releases it.

A pointer in your code says nothing about which of these cases it is. The
question to ask of every one is whether it represents a reference you own:

| Where the pointer came from | What you hold | What you must do |
|---|---|---|
| `gtk_application_new`, `g_key_file_new` | a full reference | unref it when done |
| `gtk_button_new`, any widget constructor | a floating reference | give it to a parent |
| `gtk_widget_get_parent`, `g_object_get_data` | a borrowed pointer | nothing; do not keep it past the owner |
| `user_data` in a handler | whatever you passed at connect time | make sure it outlives the connection |

### Destruction in two steps

When the last reference goes, GObject destroys an object in two steps.
**Dispose** drops the object's references to other objects; for a widget
that includes its children, which is how removing a box destroys everything
inside it. **Finalize** frees the object itself, and on the way it clears
the data attached to the object, calling each destroy function.

![The life of the counter button. Appending sinks its floating reference; removing it drops the count to zero, and finalize runs the destroy function that deletes the `Counter`.](diagrams/ch02-lifecycle.svg)

The Debugger drill shows the second step from the inside: `~Counter` is
called from `g_datalist_clear` inside `g_object_unref`.

### Signals

A signal is a named event on an object: `clicked` on a button,
`close-request` on a window. You connect a C function to it with
`g_signal_connect(object, "clicked", G_CALLBACK(handler), user_data)`, and
when the object emits the signal GObject calls your function with the object,
the signal's own arguments, and your `user_data` pointer.

Emission is synchronous. `g_signal_emit` calls every connected handler, in
order, before it returns; there is no queue. GObject also holds a reference
to the emitting object for the whole emission. In a test where a `clicked`
handler dropped the button's last reference, the button was finalized only
after the handler returned and before `g_signal_emit` did. The emitter is
always alive inside its own handlers; other objects are not.

That `user_data` pointer is where lifetime bugs come from. GObject stores it
and hands it back; it does not know what it points at and does not keep it
alive. If it points at something that is destroyed before the signal fires,
your handler gets a dangling pointer.

`g_signal_connect_object` is the fix for the most common case, where
`user_data` is itself a GObject. It ties the connection to that object's
lifetime: when the object is destroyed, the handler is disconnected, so it
can never be called with a dead pointer. It protects that one object only.
Anything else the handler reaches through it still needs its own answer to
"who keeps this alive?".

| Connection | `user_data` is | Safe when |
|---|---|---|
| `g_signal_connect` | anything | `user_data` outlives the emitting object, or you disconnect first |
| `g_signal_connect_object` | a GObject | always, for that object |
| `g_signal_connect` with `nullptr` | nothing | always; the handler finds its state on the emitter |

The third row is this chapter's counter button: its handler takes no
`user_data` at all and finds its state on the object that emitted the
signal, which cannot be dead while it is emitting.

### Data attached to objects

`g_object_set_data_full(object, "key", pointer, destroy)` attaches your own
data to any GObject, with a function to call when the object is destroyed (or
the key is replaced). It is how a widget can own a C++ object: attach it,
and give `delete` as the destroy function. You will use exactly this in
Chapter 8 to make a notebook page own everything behind its tab.

This is the bridge between the two lifetime systems in a GTK program written
in C++. C++ objects live by scope and `unique_ptr`; GObjects live by
reference count. Attaching a C++ object to a GObject with a destroy function
makes the reference count decide when the destructor runs, so you never
have to find the right moment to `delete` it yourself.

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

## Review

1. `gtk_box_append(box, button)` is followed by no `g_object_unref`. Why is
   that not a leak, and what would an unref there do?
2. Why does `on_remove_clicked` look up the parent before calling
   `gtk_box_remove`, and why must it not use `count` after the call?
3. A window has no parent. What keeps it alive, and what releases it?
4. In the Debugger drill, `~Counter` is called from `g_object_unref`. Trace
   the chain from the click on Remove to that destructor.
5. The counter button's handler is connected with `nullptr` as `user_data`.
   Why is that the safest of the three connection styles?
6. `g_signal_connect_object` protects the remove button's handler. Exactly
   which pointer does it protect, and what would it not protect?
7. With plain `g_signal_connect`, a second click on Remove produced
   `invalid unclassed pointer in cast to 'GtkWidget'` and ASan said nothing.
   Which part of GObject caught the bug, and why could ASan not?
8. You need the counter button again after removing it, to put it in a
   different box. What do you do before `gtk_box_remove`, and what do you
   owe afterwards?

**Questions for deeper thought.** Would you store a device inventory on the
list widget that shows it, or in a separate object? What happens to that
choice when a background operation finishes after the widget has been
removed? If every C++ object in a tool is owned by some GObject through
attached data, what does the ownership tree look like, and where would you
look first for a leak?

## Exercises

1. **Two counters.** In `on_activate` in `ch02-gobject/src/main.cpp`, add a
   second counter button with its own `Counter` and its own remove button.
   Done when removing one prints only its own `Counter freed after N clicks`
   and the other keeps counting.
2. **Keep it alive.** Change `on_remove_clicked` to take a reference with
   `g_object_ref` before removing the counter, and append it back to the box
   from a third "Restore" button, then unref it. Done when the count
   survives a remove and restore, and `~Counter` runs only when the window
   closes.
3. **Watch the two steps.** In `on_activate`, register a weak reference on
   the counter button with `g_object_weak_ref`, using a capture-less lambda
   that prints `weak notify`. Done when Remove prints `weak notify` before
   `Counter freed after N clicks`, and you can say which of dispose and
   finalize each line comes from.
4. **A weak pointer.** Store the counter button in a `static GtkWidget *`
   and register it with `g_object_add_weak_pointer`. Add a button that
   prints whether the pointer is null. Done when it prints non-null before
   Remove and null after, with no crash.
