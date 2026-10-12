# Appendix F — Review answers

Short answers to each chapter's Review questions. Check your own answer
against the chapter first; these say what a good answer must contain.

**Chapter 0**

1. pkg-config failed: Meson finds GTK by asking pkg-config, which reads the
   `.pc` file that the development package installs. Install `libgtk-4-dev`
   (or `gtk4` with Homebrew); `meson.build` is not the problem.
2. The compiler gets its include paths and `-D` flags from the command Ninja
   runs; the editor has no such command unless it reads
   `compile_commands.json`. With it, IntelliSense sees the same GTK headers
   and the same version-floor macros as the compiler.
3. `GDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_8`, made fatal by
   `-Werror=deprecated-declarations`. It reports any function newer than
   4.8 as "not available", so the tool cannot quietly depend on API that
   GTK 4.8 machines lack.
4. `MIN_REQUIRED` stops the headers flagging functions that were deprecated
   after 4.8, because on 4.8 they are the normal API. Without it, it
   defaults to the installed version (4.14), which is above `MAX_ALLOWED`,
   so the headers stop the build at once with
   `#error "GDK_VERSION_MAX_ALLOWED must be >= GDK_VERSION_MIN_REQUIRED"`.
   With a matching `MAX_ALLOWED` removed too, a `gtk_widget_show` call
   (deprecated in 4.10) would become an error under
   `-Werror=deprecated-declarations`.
5. No. The packaged GTK ships without debug information, so its internal
   functions have no names; exported ones such as `g_object_unref` still
   do. Use the **gtklab (gdb)** configuration with `DEBUGINFOD_URLS`
   set when you need to step into GTK; for bugs in your own frames it is not
   needed.
6. **gtklab: stop on GTK criticals** (`G_DEBUG=fatal-criticals`). It turns
   the warning into an abort, so the debugger stops with the offending call,
   and your line, on the stack instead of letting the program carry on in a
   bad state.
7. Each build directory holds one configuration (build type, sanitizer,
   which GTK). Keeping them separate means switching between them costs
   nothing and none of them is ever rebuilt from scratch because another was
   configured differently.

**Chapter 1**

1. `main` runs before the main loop and before GTK has opened the display,
   so nothing that draws can be built there. `activate` is emitted from
   inside `g_application_run` once the application is ready to show
   windows.
2. The main loop inside `g_application_run`, kept running because the
   window is registered with the application. When the last registered
   window closes, the application quits and `g_application_run` returns.
3. With `G_APPLICATION_NON_UNIQUE` the second launch is an independent
   process with its own `startup` and `activate` and its own window.
   Without it, the second launch finds the first process through its
   application id on D-Bus, asks it to activate, and exits; the first
   process opens a second window.
4. The default flags (`G_APPLICATION_DEFAULT_FLAGS`). `on_activate` then
   runs more than once in the same process, so it must create a new window
   each time and must not assume it is the first or only window.
5. `close-request` asks a question whose answer changes what GTK does next;
   `clicked` only notifies. Returning `FALSE` lets the close go ahead: GTK
   destroys the window, and if it was the last one the application quits.
6. A frozen window that does not redraw or respond. The handler runs inside
   the main loop, and the loop cannot process input or draw a frame until
   the handler returns.
7. No. `g_application_run` returns only after the main loop ends, and the
   loop ends when the last window has closed.

**Chapter 2**

1. A new widget's reference is floating, and `gtk_box_append` sinks it, so
   the box owns the only reference. An extra `g_object_unref` would destroy
   the button while it is still in the box, and the crash would come later,
   somewhere else.
2. It needs the box to call `gtk_box_remove`, and after the call the box's
   reference, the only one, is gone: the button is destroyed inside the
   call, so `count` points at freed memory.
3. GTK keeps every window in its list of toplevel windows; a new window is
   not floating. Closing the window (or `gtk_window_destroy`) releases it.
4. The click emits `clicked` on Remove, which runs `on_remove_clicked`; it
   calls `gtk_box_remove`, the box drops the last reference, the count
   reaches zero, GObject disposes and then finalizes the button, finalize
   clears the button's attached data, and the destroy function (the lambda)
   deletes the `Counter`.
5. The handler finds its state on the emitting object, and GObject keeps the
   emitter alive for the whole emission, so there is no pointer that can
   outlive what it points at.
6. The `user_data` object, the counter button: the handler is disconnected
   when it is destroyed. It does nothing for any other pointer the handler
   might reach, such as a C++ object it looks up elsewhere.
7. GObject's checked cast (`GTK_WIDGET(user_data)`) found that the memory no
   longer held a valid instance. ASan could not, because the freed memory
   belonged to GLib's allocations in libraries not built with ASan.
8. Take a reference with `g_object_ref` before removing it, so removal drops
   the count to one instead of zero. After appending it to the new box
   (which takes its own reference), call `g_object_unref` once.

**Chapter 3**

1. A container's size depends on its children's sizes, so measurement has
   to start at the leaves; a child's rectangle depends on how its parent
   divided the space, so allocation has to start at the top.
2. Its slot in the box gets a share of the spare height, because it
   expands; `START` then gives the label only its natural height, placed at
   the top of that slot. With `FILL` it would be stretched over the slot.
3. Expansion propagates upward. The scrolled window inside the box has
   `vexpand` set, so the box (and the paned above it) computes that it
   wants to expand vertically too, because neither has `vexpand` set
   explicitly.
4. Its natural height is not its content's; it reports a small natural size
   because its content is meant to scroll. Give it `vexpand` so it receives
   the box's spare height.
5. The stack keeps both pages alive and only changes which one is visible,
   so nothing is destroyed and rebuilt, and the notebook keeps its state;
   with gtklab's settings the stack also measures only the visible page.
6. A homogeneous stack is as large as its largest page, so it is allocated
   the hidden page's minimum width; the visible page is laid out in that
   wider space and the pane shows only part of it, so it appears off
   centre.
7. `resize` says whether the child grows when the paned grows; `shrink` says
   whether the divider may squeeze it below its minimum size. Both default
   to on, so a sidebar would widen with the window and could be dragged
   out of existence.
8. A size request only raises the widget's minimum. With `shrink` on, the
   paned is allowed to give the child less than its minimum, so the request
   never comes into play.

**Chapter 4**

1. Formatting `user@host:port` is core: it needs only the session's fields
   and can be tested as a string. Setting the tooltip on a widget is the
   GTK layer: it needs the widget.
2. The core target in `meson.build` is not given the GTK dependency, so the
   compiler cannot find `gtk/gtk.h` and the build fails. A build rule is
   checked on every compile; a comment is checked only when someone
   remembers it.
3. At the display. GLib is a portable C runtime (key files, file I/O, path
   and UTF-8 helpers) that needs no window; GTK needs a display, and code
   that needs one cannot be tested without one.
4. The tests would depend on the shell of whoever runs them, so a test that
   passes on one machine could fail on another. Passing the shell in makes
   the input visible and fixed.
5. The core's results end up in GTK callbacks, which are called from C. An
   exception escaping a callback unwinds through C frames not built for it;
   an error value can be checked and shown by the GTK layer.
6. The label would become the source of truth. The count would depend on
   the label's exact wording, could not be tested without a window, and
   would break as soon as the text changed; the model exists so the label
   is only output.
7. A core test answers whether the behaviour is right (does port 2222 give
   `-p 2222`?) quickly and on any machine. Only a check through the window
   answers whether GTK is wired to that behaviour (does the click start the
   right session?).
8. So the core stays plain C++ values that are copied, compared and tested
   with no GObject machinery. The wrapper is an adapter at the list level:
   it holds a copy of a core value so a GTK list model can display it.

**Chapter 5**

1. `bind`, then `unbind`. The name and the address come from the same item
   in the same `bind`, so a mismatch means a row kept text from a previous
   item: a label `bind` did not set, or one `unbind` did not clear. The
   model holds whole sessions and cannot mix two of them.
2. `setup` creates widgets; `bind` only fills them. Rows are reused, so
   building in `bind` adds a new set of widgets every time a row is
   re-bound: slow, and a leak of the widgets it replaces.
3. Anything tied to the old item: a signal handler connected to the item, a
   timer started for it, a reference to it stored on the row. Left in place,
   it keeps acting on, or keeping alive, a session the row no longer shows.
4. `GListModel` is a list of GObjects, so it can reference-count them and
   check their type. gtklab wraps `Session` so the core stays plain C++ with
   no GTK or GObject, testable on its own (Chapter 4); the wrapper exists
   only at the GTK boundary.
5. The selection takes ownership of the window's only reference. When the
   window's destructor unrefs `store_` as well, the store is released one
   time too many when the window closes: a use after free.
6. The store took its own reference in `g_list_store_append`. `unref` gives
   back the caller's reference only; the item is finalized when its last
   reference, the store's, is gone.
7. `session` would be an unconstructed `Session` in zero-filled memory; the
   first assignment to it is undefined behaviour. The compiler cannot see it
   because GObject allocates the struct with C calls and never runs a C++
   constructor.
8. For a short list whose rows differ from each other or carry their own
   controls, such as a settings page or a few dozen entries. A list that
   grows with the user's data should be a `GtkListView`.

**Chapter 6**

1. In the data. Each folder item carries its sessions (gtklab's
   `GtklabSessionItem` holds a `Folder`); the tree list model only asks for
   a child model when it needs one, and inserts rows only while the folder
   is expanded.
2. A `GtkTreeListRow`. `gtk_tree_list_row_get_item` returns your
   `GtklabSessionItem` inside it, as a new reference that must be released.
3. The expander has to know whether to draw a triangle, so binding a row
   asks `gtk_tree_list_row_is_expandable`, which calls the child function.
   It therefore runs for every row shown, at startup and again on expand,
   and must be quick and safe to call repeatedly.
4. The expander would keep the old row: a recycled widget could show the
   previous row's indent and triangle, and the expander would hold that row
   until the next bind.
5. A filter over the flattened rows only sees rows that exist, so sessions
   in closed folders cannot match and non-matching folders hide their
   children. Filtering in the core can express exactly the rule wanted and
   is already tested without GTK; rebuilding the root from its result is
   simple.
6. The splice replaces every folder item with a new one, and new rows start
   collapsed (`autoexpand` is `FALSE`). Positions change whenever a folder
   above opens or a filter drops folders, so only a stable key such as the
   folder's name identifies the same folder before and after.
7. Two folders to re-open, the first with sessions in it. Expanding the
   first inserts its sessions after it, which pushes the second folder past
   the count read before the loop, so the loop ends before reaching it.
8. The folders that were open when the search began. `user_expanded_` is
   captured on the first keystroke and restored on clear; what happened to
   the filtered tree in between is thrown away by the rebuild.

**Chapter 7**

1. The parameter says what to do this time and arrives with each
   activation; the state is what is true now and stays on the action.
   `win.toggle-sidebar` has only a boolean state; `win.app-variant` has a
   string parameter and a string state.
2. The prefix (`win.` or `app.`) and the spelling of the name against the
   action map; then whether the action was added to the map the control can
   reach (the window, not some other object); then whether it is disabled.
   None of these prints a warning.
3. The first press asks for `false`: the sidebar hides, but the state stays
   `true`, so the check mark stays on. The second press asks for `false`
   again, and nothing changes: the sidebar cannot be shown from the action
   any more.
4. The state is parsed as GVariant text, and a string in that format needs
   its own quotes. Without them, `light` is not a valid GVariant value.
5. Find the window it should act on, since its user data could not be one
   `AppWindow`; with several windows it would have to decide whether to
   close one or all of them.
6. `gtk_application_set_accels_for_action` makes the menu show the key, and
   it also runs the action from the window's capture phase; the window's
   capture-phase `GtkShortcutController` runs it too. Whichever handles the
   key first stops it, so the action runs once.
7. A pending timeout holds `this`; if the window is destroyed first, it
   would fire on freed memory. Returning `G_SOURCE_REMOVE` destroys the
   source, so the id is stale; zeroing it stops a later `g_source_remove`
   on a source that no longer exists, which is a critical warning.
8. The second window's `activate` adds a new `refresh` to the application,
   replacing the first one, so Refresh from either window updates only the
   second window's label. Window state belongs in a `win.` action.

**Chapter 8**

1. Reloading replaces the window's `folders_` vector and rebuilds the tree's items, so a pointer into either would point at freed memory. The tab's own copy stays valid and keeps the host and user it was opened with.
2. Nothing. The notebook is the only list of tabs, and each page carries its own `Tab`, so the new order is simply the notebook's order. A registry beside the notebook would now be out of order.
3. Because a stored pointer goes stale when that tab is closed or the pages are reordered, and every path would have to remember to update it. Asking the notebook for its current page when the command runs is always right.
4. Not the new page: in the measured runs it returned the old page's number, or -1 while the current page was being removed. Use the `page` argument the signal passes.
5. The close button, Close Tab and Close Other Tabs (Chapter 10), the process exiting (Chapter 11), and the window closing with tabs open. Every one of them ends with the page leaving the notebook and being finalized, so `~Tab` is the one place every path reaches.
6. `page-removed` runs at `remove_page`, and the page is left with no parent and its `Tab` still attached. `~Tab` runs only when the extra reference is dropped and the page is finalized.
7. By the time the page is finalized and its data cleared, its child view has already been finalized; the measured order was view finalized, page destroy, `~Tab`. `tab->view` is a dangling pointer in the destructor.
8. If the page went first by some other path while the button could still be clicked, the handler would be called with a pointer to a dead page. `g_signal_connect_object` disconnects the handler when the page goes.

**Chapter 9**

1. First the page's capture-phase click gesture, which denies button 1 (`page capture: button 1 denied`); then, with the press still unclaimed, the text view's own gestures place the cursor and the bubble-phase logging gesture on the view prints `text view (bubble): button 1`.
2. The capture phase runs from the window down before the target's controllers, so the page claims the sequence before the text view's menu gesture gets it. In the bubble phase the text view, as the target, sees the press first and handles it, so the page's gesture never gets the chance.
3. The plain wheel would no longer scroll the text view: the scroll would be reported as handled and swallowed before reaching the scrolled window.
4. The focused widget in the tree is the target, and propagation only visits the target and its ancestors. The notebook is not an ancestor of anything in the sidebar, so its controllers are never on the path; a shortcut that must work everywhere goes on the window, in the capture phase.
5. No. A target-phase controller runs only when its own widget is the target; a click on the text view targets the text view. The test program's box never ran its target-phase gesture for a click on its child.
6. The controllers belong to the page and the page owns the `Tab`; they are torn down together, so no handler can run with a deleted `Tab`.
7. `gtk_event_controller_get_current_event_state` gives the modifiers recorded in the event being handled. A separate tracker is extra state that can get out of step with the event actually being handled, for no gain.

**Chapter 10**

1. Close Tab removes that page, and a popover dies with its parent, so the menu would be destroyed by the very action it was dispatching. Parenting it to the notebook, which outlives every tab, means not depending on how GTK copes with that.
2. The popover emits `closed` before it activates the chosen item's action. Unparenting in `closed` destroys the menu first, so the chosen item (Copy in the Break it) silently does nothing.
3. The gesture is attached to the row's widget, so `x, y` are in the row's coordinates. Without conversion the menu is placed as if the row's origin were the list view's, near the top of the list instead of at the pointer.
4. Actions on tabs already resolve "the current tab", and making the clicked tab current is what a user expects. A row is not current in any sense the actions know, and rows are recycled, so the row menu names its session explicitly as the action's target.
5. A modal dialog stops input to its parent window. The main loop keeps running: timers, redraws and, from Chapter 11, output to other tabs all continue while the question is up.
6. The tab may be closed another way while the dialog is up; a `Tab *` could then point at freed memory, while the reference keeps the page (and so its `Tab`) alive. When the answer comes it checks that the page still has a parent, that is, it is still in a notebook, before closing it.
7. `GTK_RESPONSE_DELETE_EVENT` (-4) in the recorded run. `confirm` acts only on `GTK_RESPONSE_ACCEPT` and treats everything else as no, so Escape, Cancel and closing the dialog need no cases.
8. Each layer runs in its own callback from the main loop: the right-click, the later click on the menu item that runs the action, and the dialog's response even later. That is why each log line carries the target name.

**Chapter 11**

1. The timer and the descriptor callbacks run, because both are at priority
   0 and that is the highest ready priority. The idle, at 200, waits for an
   iteration in which nothing more urgent is ready.
2. The budget makes the callback return, but at the default priority the pty
   is ready again on the next iteration and still outranks the redraw at 120,
   so it is dispatched again and the frame never runs. Only a priority below
   the redraw lets the frame in.
3. An idle is always ready, so the loop's poll timeout is zero and the loop
   calls it again immediately, forever. Descriptor watches are ready only
   after `poll()` reports data, so the thread sleeps in `poll()` until then.
4. From the other side of the pty: the line discipline echoes input, or the
   shell's line editor does, and the echo comes back through the master like
   any other output.
5. The kernel's line discipline: with `ISIG` set, the interrupt character
   (byte 3) becomes `SIGINT` for the foreground process group of that
   terminal, here `sleep`. gtklab is not in that session and receives
   nothing.
6. Without the idle, removing the page deletes the `Tab` and its
   `PtySession` while the session's exit callback is still running, a
   use-after-free. Without the reference, the page could be destroyed (say,
   by the user closing the tab) before the idle runs, and the idle would use
   a dangling pointer.
7. The source is already destroyed; `~PtySession` would call
   `g_source_remove` with a stale id, which GLib reports as a critical
   warning.
8. One save arrives as several events (write, rename, permission change).
   Reloading on the first would read the file before the editor finished
   writing it; restarting the timer means the reload happens once, 300 ms
   after the last event, when the file has settled.

**Chapter 12**

1. `update_grid` divides the pixels by the cell size: 637 / 9 gives 70 whole
   columns, and the grid resizes its cell model to that and emits `resized`.
   The 7 pixels left over are a margin at the right that `snapshot` fills
   with the background colour.
2. The cell size would change but no layout pass would run, so
   `size_allocate` would not be called, `update_grid` would keep the old
   column and row counts, and the pty would not learn the new size; the grid
   would draw new-size cells into an old-size model.
3. GTK keeps each widget's last recording and asks for a new one only after
   `queue_draw` (or a new size). Nothing queued a draw, so the old recording
   was still valid as far as GTK knew; the resize asked for a new one.
4. Cells are what the pty needs (`TIOCSWINSZ` is in rows and columns), and
   pixels are the widget's private business. Allocations happen for many
   reasons that do not change the shape, and only a shape change is news to
   the child.
5. A widget whose size in one direction depends on the other, such as a
   wrapping label that gets taller as it gets narrower. A grid's minimum and
   natural sizes are fixed by its cell, whatever the other dimension is.
6. The cell is the size of one character as it will be drawn. If the
   measurement used different attributes (a different font, ligatures on),
   the cell would not match the glyphs `paint_row` draws and text would
   drift off the grid.
7. They do not reach it during `on_readable`: feeding only changes the cell
   model and queues a draw. The text appears on the next frame, which runs
   after `on_readable` returns and the main loop lets the frame clock in.
8. GObject zero-fills the instance and runs no constructors, so the member
   would be zeroed bytes rather than a valid string; the first assignment to
   it would be undefined behaviour.

**Chapter 13**

1. `lower` is 0, `upper` is 5,027 (every line held), `page_size` is 27 (the
   rows on screen), and `value` is 5,000, the first line of the screen.
2. The widget's side: the grid must put every change in its line count into
   the adjustment. The scrolled window only knows what the adjustment tells
   it, so it went on describing one screen of lines and had nothing to
   scroll to.
3. Output arrives on its own, and a user who scrolled back to read something
   should not be pulled away from it. A key press is the user acting at the
   prompt, so showing the prompt is what they want.
4. Every terminal line is one cell high, so in lines the first line drawn is
   simply `value` and a selection can be stored as absolute line numbers.
   A text view's lines differ in height, so only pixels describe its
   content exactly.
5. The grid sends `\r`; `cat` receives `\n`, because the pty's line
   discipline turns carriage return into newline on input (the `ICRNL`
   flag). bash's line editor turns that flag off while it waits at the
   prompt, so it reads the `\r` and treats it as Enter.
6. The window's shortcut controller runs in the capture phase, before the
   grid's key controller, and handles the key, so it never reaches the grid.
   Plain Ctrl combinations belong to the program in the terminal (Ctrl+C
   interrupts, Ctrl+V is a literal-next in the shell), so a terminal that
   took them would break the programs it runs.
7. Without the reference, closing the tab would destroy the grid and the
   callback would then run on freed memory. With it, the grid stays alive
   until the callback has run and dropped the reference.
8. `on_paste_ready` replaces each newline with `\r`, the byte Return sends,
   so each line reaches the shell as if Enter were pressed. With bracketed
   paste, the terminal wraps the paste in `ESC [200~` and `ESC [201~` when
   the program has asked for it, and the program inserts the text instead of
   running it.

**Chapter 14**

1. The session never touches a widget: it hands output to a handler the tab
   sets, and the tab connects the widget's `commit` and `resized` to its
   `write` and `resize`. gtkpyte has a feed function and the same two
   signals, so only the wiring in `start_tab` changes.
2. Nothing special: pyte's parser keeps its state (here, collecting CSI
   parameters) between calls, so the next `feed` continues from that state.
   GtklabGrid had to buffer the incomplete sequence and scan it again, which
   works only for the sequence shapes it recognises.
3. pyte builds a cursor position report, `ESC [ row ; col R`, and gtkpyte
   emits it through `commit`, which the tab writes to the pty like a
   keystroke. With nothing connected, the reply is lost and the program
   waits for an answer that never comes.
4. `vim` runs on the alternate screen, which keeps no scrollback. With
   `set mouse=a`, `vim` has asked for mouse reports, so each wheel notch is
   sent to it as a button 64 or 65 press. Without it, alternate scroll (mode
   1007, on by default) turns the wheel into Up and Down arrow keys.
5. By holding Shift. The capture-phase report gesture's `on_report_begin`
   denies the sequence when Shift is held, so the press continues to the
   selection gestures.
6. gtkpyte's source directory, on the include path of anything that uses
   `gtkpyte_dep`, contains its own `pty.h`, which would shadow the system
   `<pty.h>` and hide `forkpty`. Built separately without `gtkpyte_dep`, the
   pty code sees the system header.
7. It cost nothing measurable: the 67 MB `cat` took about the same time
   (16.5 s against 17.9 s). It bought a terminal that repaints every second
   during the flood instead of once in six seconds, because the redraw at
   priority 120 now outranks the pty watch.
8. The raw bytes the child sent, escape sequences included. The screen shows
   the result of interpreting them (colours, cursor movement, a full-screen
   program redrawing in place), which is not the same as the stream.

**Chapter 15**

1. The bundle itself: `glib-compile-resources` turned the manifest's files into a byte array in the binary's `.gresource.gtklab` section, plus a constructor. The constructor runs before `main` and registers the bundle, so any code can then read `/io/github/gtklab/...` by path.
2. Meson regenerates `gtklab-resources.c` from the manifest, compiles that one file and relinks gtklab. The core is untouched because it contains no theme data; it only parses whatever bytes it is given at run time.
3. Red. Provider priority is decided before specificity, so the application-priority rule wins over the more specific theme rule; a broad selector in `style.css` would therefore override the theme on every matching widget.
4. The terminal colours are content the user chooses, independent of the application's light or dark variant, not chrome the theme should decide. They are data, parsed and tested in the core, and handed to gtkpyte's own colour API, which queues a redraw.
5. Its name ends in `-symbolic`, so GTK forces every `rect`, `circle` and `path` to the widget's CSS `color`, which `style.css` sets to `@error_color` on `image.log-indicator`. Setting `color` to a green in that rule (or in the Inspector) makes it green.
6. It is no longer treated as symbolic, because only names ending in `-symbolic` are recoloured, so it draws as written: a dark grey dot, in light and dark alike.
7. The system theme is searched before hicolor, where resource paths live, so the system's icon is shown and gtklab's is never used. That is why gtklab prefixes its names with `gtklab-`.
8. Parsing is in the core: `parse_theme` takes a string and reads the key file with GLib and checks each colour against `#rrggbb` itself, so it can name the failing key in its error and be tested by doctest with no display.

**Chapter 16**

1. Use after free: the watch's callback reads a `PtySession` whose memory has been returned. A normal build usually shows nothing, or crashes later somewhere else; the ASan build stops at the first read with `heap-use-after-free` and the access, free and allocation stacks, all in gtklab's code.
2. In Chapter 8 nothing that was read had been freed: the `AppWindow` lived until the window's finalization, and the stack was being dismantled but was still allocated, which is use during destruction. In the Break it, the freed stack was dereferenced only inside GLib's type check, which is not instrumented, and the `AppWindow` itself was valid memory mid-destructor; GLib's check produced the critical instead.
3. Object-data destroy functions run in `finalize`, the very last step. By then `dispose` had already released the window's children, and their teardown emitted `page-removed` into an `AppWindow` that was still connected and still believed the widgets were whole.
4. Not safe: `destroy` is emitted when the window is disposed, and GTK does not promise that its children are still there. It becomes safe if the code holding the handler took its own reference to the label beforehand and releases it afterwards.
5. That the notebook is alive, undisposed and still holding its pages when the destructor runs, whatever order GTK tore the rest down in. Disconnecting first means that when the last reference goes and the notebook removes its pages, no handler holding `this` hears it.
6. If freed memory were reused at once, the next allocation would refill it with valid-looking data and a stale read would see nothing wrong. Quarantine keeps it poisoned for a while, so the read hits poison and is reported.
7. A GTK or GLib `CRITICAL` was logged, and `fatal-criticals` made GLib raise `SIGTRAP` after printing it; with no debugger attached, that killed the process. Run the same configuration under the debugger (**stop on GTK criticals**) and read the stack at the stop.
8. The idle holds `this` until it runs. If the window is destroyed between the tab switch and the next idle, the callback would run on a deleted `AppWindow`; with the id stored, the destructor can remove it.

**Chapter 17**

1. GTK's header marks it `GDK_AVAILABLE_IN_4_10`; with `GDK_VERSION_MAX_ALLOWED` at 4.8 that expands to a deprecation attribute ("Not available before 4.10"), and `-Werror=deprecated-declarations` makes the warning an error.
2. `GDK_VERSION_MIN_REQUIRED` is 4.8, so deprecations made after 4.8 expand to nothing. The API is valid on the floor, and the macros keep it quiet on newer GTK.
3. For example, a call to a Pango or cairo function newer than Debian 12's, made from gtkpyte, which no version macro guards; or code that relies on how a newer GTK measures or orders its signals. Both compile on your desk.
4. The prefix pairs GTK 4.8.3 with the build machine's GLib 2.80, a combination Debian never ships, and the two disagree inside GTK. Debian 12 pairs GTK 4.8 with GLib 2.74, so it is the truer floor; the prefix is a convenient approximation.
5. The program ran against the system GTK: `LD_LIBRARY_PATH` was set only inside the script, so the run needs the value the script printed, otherwise the dynamic linker picks the system library.
6. A Wayland desktop matches the window's app id, which is the application id, against desktop-file names. On X11 the window's `WM_CLASS` is the program name, `gtklab`, so `StartupWMClass` tells the desktop which class belongs to this entry. Without the match the dock shows a second, generic icon.
7. Yes. Meson's `b_ndebug` defaults to `false` for every build type, so `NDEBUG` is not defined in a release build unless you set `b_ndebug`.
8. When you expect to debug a problem on that person's machine: `debugoptimized` is optimised but keeps debug information, so a stack from it still has function names and lines.
