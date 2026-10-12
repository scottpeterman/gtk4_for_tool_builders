# Appendices {.part}

# Appendix A — GTK 3 to GTK 4

Most GTK material online, and most answers to most questions, are about
GTK 3. Translate with this table before copying anything. The full guide is
*Migrating from GTK 3.x to GTK 4* in GTK's documentation.

| GTK 3 | GTK 4 | Chapter |
| --- | --- | --- |
| Widgets hidden until `gtk_widget_show_all` | Widgets visible by default; `gtk_widget_set_visible` | 1, 3 |
| `gtk_container_add`, `GtkContainer` | Per-container calls: `gtk_window_set_child`, `gtk_box_append`, `gtk_paned_set_start_child` | 3 |
| `gtk_widget_destroy` on any widget | Remove it from its parent; `gtk_window_destroy` for windows | 2, 3 |
| `GtkTreeView` with `GtkListStore` | `GListModel` with `GtkListView` or `GtkColumnView` and a factory | 5 |
| `GtkTreeView` with `GtkTreeStore` | `GtkTreeListModel` with `GtkTreeExpander` | 6 |
| `GtkMenu`, `GtkMenuItem`, `GtkUIManager` | `GMenu` model, `GAction`, `GtkPopoverMenu` | 7, 10 |
| `gtk_widget_add_accelerator`, `GtkAccelGroup` | `GtkShortcutController`, or accelerators on actions | 7 |
| `button-press-event`, `key-press-event` signals | Event controllers and gestures, with propagation phases | 9 |
| `gtk_menu_popup_at_pointer` | A `GtkPopoverMenu` parented to a widget, positioned with `set_pointing_to` | 10 |
| `gtk_dialog_run` | The `response` signal; dialogs are asynchronous | 10 |
| `GtkFileChooserDialog` | `GtkFileChooserNative` at the 4.8 floor; `GtkFileDialog` from 4.10 | 10 |
| `draw` signal with cairo | `snapshot` virtual function with `GtkSnapshot` | 12 |
| `get_preferred_width` / `_height` | `measure`, called once per orientation | 12 |
| `size-allocate` signal, `gtk_widget_get_allocation` | `size_allocate` virtual function; `gtk_widget_get_width` / `_height` | 12 |
| `GtkStyleContext` for adding classes | `gtk_widget_add_css_class` / `remove_css_class` | 15 |
| `gtk_widget_translate_coordinates` | `gtk_widget_compute_point` (both exist in 4.8; the first is deprecated from 4.12) | 10 |

# Appendix B — Debugger recipes

Each recipe uses one of the launch configurations from Chapter 0. The chapter
column is where the symptom first appears in the book.

| Symptom | Recipe | Chapter |
| --- | --- | --- |
| A `CRITICAL` scrolls past in the terminal | Run **stop on GTK criticals** (`G_DEBUG=fatal-criticals`); go up the stack to the first frame of your own code | 0, 2 |
| A widget is the wrong size, or collapsed to nothing | **gtklab + GTK Inspector**: select it with the picker, check `hexpand`, `vexpand` and its allocation; check its parent's shrink settings | 3, 5 |
| `GtkGizmo ... reported min height -3` | A homogeneous `GtkStack` measuring a hidden notebook with no tabs; make the stack non-homogeneous | 3 |
| A handler never runs | Inspector, Controllers tab: check the controller's phase, and whether something earlier claims the event or returns `TRUE` | 9 |
| A menu item is grey | The action name's prefix (`win.` or `app.`) and its parameter type must match exactly | 7 |
| A menu item does nothing, and nothing is logged | The popover was unparented inside its `closed` handler; unparent on idle | 10 |
| Typed keys go nowhere | The widget that was clicked, or the closing popover, took focus back; move focus with `focus_later` | 11, 13 |
| The window stops repainting under heavy output | The descriptor watch's priority first, then the read budget | 11, 14 |
| Nothing appears until the window is resized | A model change without `gtk_widget_queue_draw` | 12 |
| A crash that comes and goes | The ASan build, **gtklab (ASan)**; read the free and allocation stacks | 16 |
| A `CRITICAL` at quit, and ASan is silent | A handler running inside a destructor; read the fatal-criticals stack for your destructor, then disconnect by data before letting go | 16 |
| You want to step into GTK itself | The **gtklab (gdb)** configuration, with `DEBUGINFOD_URLS` exported before VS Code starts | 0 |
| A bug that only your users see | Build against the floor; check `gtk_get_minor_version()` in the Debug Console | 17 |

# Appendix C — Recall answer key

**Chapter 5.** The core library owns the session data; the window only
displays it. A new widget starts with a floating reference, which the first
container it is added to sinks and keeps, so the container owns it.

**Chapter 6.** `setup` creates the row's widgets, once per row widget; `bind`
only fills them, and runs every time a row widget is reused. Because
`GListModel` holds GObjects and nothing else. `src/core/session.h` and
`session.cpp`, and they do not include GTK.

**Chapter 7.** For every visible folder at startup, so that the expander
knows whether to draw a triangle, and again for several folders on each
expand and collapse; so it must be cheap. Setup, then bind, then unbind, with bind and unbind repeating
as the row widget is reused. Which folders were open: `capture_expanded`
records their names before the rebuild, and the rebuild expands them again.

**Chapter 8.** That its destroy function runs when the object is finalized,
or when the data is replaced. To coalesce a burst of changes into one write.
That the action belongs to the window, not the application.

**Chapter 9.** `page-removed`, while the page still exists, then `~Tab` as
it is destroyed. The capture phase, so that the shortcut runs before the
focused widget sees the key.

**Chapter 10.** It takes the whole sequence: no other gesture sees it.
The event carries on to the next controller, so the scrolled window
scrolls. The page, through object data.

**Chapter 11.** `closed` is emitted before the menu activates the chosen
item; unparenting there destroys the menu, and the action with it. A timeout
from `g_timeout_add`. In Chapter 8, only a debug line; from Chapter 11 it
destroys the session, which closes the pty.

**Chapter 12.** Fills a row widget, which may be reused, with an item's data.
`G_DECLARE_FINAL_TYPE` and `G_DEFINE_FINAL_TYPE`. The core.

**Chapter 13.** The capture phase, so that the page gets first refusal before
the widget inside it. At the next frame after a draw is queued, from the
frame clock. At idle priority, below GTK's redraw, so that a busy pty cannot
stop the window repainting.

**Chapter 14.** Capture, target, bubble. The read budget, and the idle
priority of the descriptor watch. `resized`.

**Chapter 15.** It asks for a frame; it does not draw, and it does not lay
the widget out again. Code that needs no display: parsing, the session and
settings models, now the theme parser; not anything that needs a widget.
`gtkpyte_terminal_set_colors`, with `set_color_cursor` and
`set_color_selection`.

**Chapter 16.** `Child name 'empty' not found in GtkStack`, when quitting
with tabs open. It disconnects the handler automatically when the object
passed with it goes away. Remove every main-loop source that holds `this`,
the descriptor watch and the child watch, before releasing anything else.

**Chapter 17.** `GDK_VERSION_MIN_REQUIRED` and `GDK_VERSION_MAX_ALLOWED`,
with the GLib pair. `GtkFileDialog` arrived in 4.10, above the 4.8 floor.
Leak detection, because GTK and its libraries keep allocations until exit by
design, and the reports bury the one you want.

# Appendix D — GObject in one page

The pieces of GObject this book uses, in the order it uses them.

| You want | Write | Chapter |
| --- | --- | --- |
| A reference you own | `g_object_ref`; give it back with `g_object_unref` | 2 |
| To keep an object that may still be floating | `g_object_ref_sink` | 13 |
| A C++ object owned by a GObject | `g_object_set_data_full(obj, key, ptr, destroy)` | 2, 8 |
| A handler that cannot outlive a second object | `g_signal_connect_object` | 2 |
| Every handler with your pointer, gone | `g_signal_handlers_disconnect_by_data(instance, this)` | 16 |
| A new final type | `G_DECLARE_FINAL_TYPE` in the header, `G_DEFINE_FINAL_TYPE` in the source | 5, 12 |
| A type that implements an interface | `G_DEFINE_FINAL_TYPE_WITH_CODE(..., G_IMPLEMENT_INTERFACE(...))` | 13 |
| C++ members in a GObject | Placement `new` in `init`, explicit destructor call in `finalize` | 5, 12 |
| Drop references to others | `dispose`; may run more than once; chain up last | 12 |
| Free everything else | `finalize`; runs once; chain up last | 12 |
| A property | `g_param_spec_*` in `class_init`, `g_object_class_install_properties` | 12 |
| A signal | `g_signal_new` in `class_init` | 12 |
| An interface's properties | `g_object_class_override_property` | 13 |
| Release and clear a member | `g_clear_object(&member)` | 5 |

# Appendix E — Source map

Each chapter's code is a complete program in its own directory. The tmssh
column is where the production version of that chapter's idea lives.

| Chapter | Directory | Start reading tmssh at |
| --- | --- | --- |
| 1 | `ch01-window` | `main` and `on_activate` in `src/main.cpp` |
| 2 | `ch02-gobject` | `add_tab` in `src/app_window.cpp` |
| 3 | `ch03-layout` | `build_ui`, `build_sidebar` |
| 4 | `ch04-core` | `tmssh_core` in `meson.build`; `tests/` |
| 5 | `ch05-listview` | `on_factory_setup`, `on_factory_bind`, `on_factory_unbind` |
| 6 | `ch06-tree` | `create_child_model`, `rebuild_tree`, `capture_expanded` |
| 7 | `ch07-actions` | `install_actions`, `build_menu`, `save_settings_soon` |
| 8 | `ch08-tabs` | `add_tab` and its `g_object_set_data_full` deleter |
| 9 | `ch09-input` | the page controllers in `add_tab`; gtkpyte's `on_report_begin` |
| 10 | `ch10-popovers` | `show_terminal_menu`, `confirm`, `act_import_file` |
| 11 | `ch11-mainloop` | `PtySession::on_readable` in `gtkpyte/src/ptysession.cpp` |
| 12 | `ch12-widget` | `gtkpyte/src/gtkpyte-terminal.cpp` |
| 13 | `ch13-input` | gtkpyte's `on_key_pressed`, `on_click_pressed`, `on_scroll` |
| 14 | `ch14-gtkpyte` | `start_tab`, `act_log`, `sync_log_action` |
| 15 | `ch15-resources` | `data/tmssh.gresource.xml`, `src/theme_apply.cpp` |
| 16 | `ch16-lifetime` | `AppWindow::~AppWindow` |
| 17 | `ch17-shipping` | `scripts/build.sh`, `scripts/appimage.sh` |
