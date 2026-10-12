# Chapter 11 — The main loop: file descriptors, idles, timers, monitors

Each tab now runs a real process on a pseudo-terminal: your shell for a
local session, ssh for a remote one. Its output appears in the tab, a line
typed under it goes to the process, and when the process exits the tab
closes itself. Along the way the sessions file starts reloading by itself
when it is edited. All of it is main loop programming, and this chapter is
where the main loop stops being background and becomes the subject.

::: {.box title="Recall"}
Why is a popover unparented on idle (Chapter 10)? What did the debounced
settings save use (Chapter 7)? What does a tab's destructor clean up
(Chapter 8)?
:::

## The idea: one thread, many sources

A GTK program runs on one thread. That thread spends its life in the GLib
main loop, and the main loop does one thing over and over: find out which
**sources** are ready, and call their callbacks. Everything your program
reacts to is a source:

- a file descriptor with data to read: `g_unix_fd_add`;
- a child process that has exited: `g_child_watch_add`;
- a timer that has expired: `g_timeout_add`;
- "nothing else to do": `g_idle_add`;
- window system events, and GTK's own work of laying out and drawing the
  window, which GTK registers for you.

Two rules follow, and the second is the one most GTK code gets wrong.

**A callback must return quickly.** While your callback runs, nothing else
does: no input, no drawing. A callback that reads everything a busy process
has written before returning freezes the window until the process stops.
The fix is a **budget**: read up to a fixed amount, then return and let the
loop come back for more.

**Sources have priorities, and the highest wins.** Every source has a
priority, a number where lower means more urgent. On each iteration, GLib
looks at all the sources that are ready, takes the highest priority among
them, and dispatches only the sources at that priority. Lower-priority
sources that are also ready wait for an iteration in which nothing more
urgent is ready.

![GLib priorities, most urgent at the top. A source that is always ready at the default priority wins every iteration, and GTK's redraw, below it, never gets a turn.](diagrams/priorities.svg)

Here is the trap. `g_unix_fd_add` watches a descriptor at
`G_PRIORITY_DEFAULT`, which is 0. GTK redraws at `GDK_PRIORITY_REDRAW`,
which is 120, *less* urgent. While a process writes faster than you read it,
which is exactly what `cat` of a large file does, the pty is ready on every
iteration, the main loop dispatches it every time, and the window never
repaints until the output stops. The read budget does not help: it makes
the callback return, but the next iteration picks the pty again.

The fix is to watch the pty at idle priority, below the redraw, with
`g_unix_fd_add_full(G_PRIORITY_DEFAULT_IDLE, ...)`. Reading still runs
whenever nothing more urgent is waiting, which is nearly always, and the
window gets a frame in between. Chapter 14 measures the difference: with a
67 MB `cat`, screenshots taken a second apart changed once in six seconds at
the default priority and every second at idle priority, for about the same
total time.

## The idea: a pseudo-terminal

A terminal program runs its child on a **pseudo-terminal** (pty): a pair of
file descriptors that behave like a serial line. The child's end looks like a
real terminal to it, so shells print prompts and programs like `vim` and
`htop` draw full-screen. Your end is the other side: you read what the child
writes and write what the user types.

`forkpty` does the work in one call: it opens a pty, forks, and in the child
makes the pty its controlling terminal and its standard input, output and
error. The parent gets the child's pid and the master descriptor. Window size
travels separately, with the `TIOCSWINSZ` ioctl, which sends the child
`SIGWINCH`.

## Build

### PtySession

The interface:

```{.cpp include="ch11-mainloop/src/pty_session.h"}
```

`start` builds the child's argument list before forking, because after
`fork` only async-signal-safe calls are allowed until `exec`. It then makes
the master descriptor non-blocking and adds the two sources: the descriptor
watch, at idle priority, and the child watch.

```{.cpp include="ch11-mainloop/src/pty_session.cpp" fn="bool PtySession::start("}
```

The read callback reads until the pty is drained or the budget is spent.
Each `read` takes up to 16 KiB; the loop stops at 512 KiB, the same budget
tmssh uses:

```{.cpp include="ch11-mainloop/src/pty_session.cpp" fn="gboolean PtySession::on_readable("}
```

The destructor is short, and its order is the point of it. Every main loop
source that holds `this` is removed before anything else happens; only then
are the descriptor and the child released. The child is still reaped
afterwards by a watch that holds nothing of the session's, or it would stay
a zombie.

```{.cpp include="ch11-mainloop/src/pty_session.cpp" fn="PtySession::~PtySession("}
```

On Linux, `forkpty` lives in `libutil`. Add it in `meson.build`:

```{.meson include="ch11-mainloop/meson.build" from="util = meson.get_compiler" to="util = meson.get_compiler"}
```

and add `util` to the executable's dependencies. On the Mac, `forkpty` is in
the C library and declared in `<util.h>`, which `pty_session.cpp` includes
there.

### A tab that runs a session

The tab owns its session as a `std::unique_ptr<PtySession>`, so deleting the
`Tab` (Chapter 8) ends the session. The output handler feeds the tab's view;
the exit handler closes the tab, but never from inside the exit callback
itself, because removing the page deletes the `Tab`, which deletes the
`PtySession` whose callback is still running. It defers to an idle, holding a
reference to the page so that it cannot disappear in between:

```{.cpp include="ch11-mainloop/src/app_window.cpp" fn="void AppWindow::start_tab("}
```

The output view is still Chapter 8's text view, for one more chapter. Text
views need valid UTF-8, and pty output is not guaranteed to be, so it goes
through `g_utf8_make_valid`; the buffer is capped at 2,000 lines. The child
runs with `TERM=dumb`, so the shell sends few escape sequences. A `GtkEntry`
under the view sends a line to the session on Enter.

### Focus, on idle

Testing found a bug here that is worth a section of its own. A tab opened by
double-clicking a session in the tree was given focus by `add_tab`, and then
lost it: the list view takes focus back when the click that activated the row
finishes. Typed text went nowhere, and the next Enter re-activated the row
and opened a second tab. The fix is to ask for focus on idle, after the click
has finished:

```{.cpp include="ch11-mainloop/src/app_window.cpp" fn="static void focus_later("}
```

### The sessions file reloads itself

A `GFileMonitor` reports changes to a file. Editors save in several steps
(write a temporary file, rename it, change its permissions), so one save
arrives as several events; in the recorded run, one write produced four.
Each event restarts a 300 ms timer, and only the last one reloads:

```{.cpp include="ch11-mainloop/src/app_window.cpp" fn="void AppWindow::on_sessions_changed("}
```

```{.cpp include="ch11-mainloop/src/app_window.cpp" fn="gboolean AppWindow::on_sessions_settled("}
```

## Done when

A local tab runs commands, `exit` closes it, and editing the sessions file
while gtklab is running updates the tree without a reload key.

![A local session running commands; after `exit` the tab has closed, and the Lab folder shows a session added to the file while gtklab was running.](images/ch11-pty.png)

::: {.box title="Debugger drill"}
Set a breakpoint in `on_readable` and run `seq 3000000` in a local tab.
Step through the loop and watch `total` grow until the budget is reached.
The scratch config's log shows the same thing without stopping:
`read budget spent after 528255 bytes; yielding`, over and over. While the
debugger is stopped, try to resize the window: it does not repaint, because
you stopped the only thread that draws it.
:::

::: {.box title="Break it"}
Remove `g_source_remove(read_watch_)` from `~PtySession`. Run `yes` in a
tab and close the tab while it runs. The descriptor watch still exists and
still holds the deleted session's address; the next time the pty is
readable, it calls `on_readable` on freed memory. In a normal build this may
crash, or may not, depending on what reused the memory. In the
AddressSanitizer build of Chapter 16 it is reported on the first try as
`heap-use-after-free`, with the stacks that allocated and freed the session
both pointing into your code.
:::

## Common mistakes

**Watching a pty at the default priority.** The window stops repainting
under heavy output. Use idle priority.

**Reading until `EAGAIN` with no budget.** The callback never returns while
the child writes faster than you read.

**Removing the tab from the exit callback.** The callback belongs to the
object you are about to delete. Defer to an idle.

**Not clearing a source id when returning `G_SOURCE_REMOVE`.** The source is
gone; removing it again later is a critical warning.

**Forgetting to reap the child.** Every child must be waited for, or it stays
in the process table as a zombie until gtklab exits.

::: {.box title="Read in tmssh"}
`PtySession::on_readable` and `kReadBudget` in `gtkpyte/src/ptysession.cpp`.
tmssh's session waits for end of file and then collects the child's exit
status on a short retry timer, rather than using a child watch, so the last
output is never cut off. Then `start_tab` and its deferred removal,
`focus_later`, `on_sessions_changed` and `watch_sessions_file`.

An exercise: tmssh watches its pty with `g_unix_fd_add`, at the default
priority. Run `cat` of a 50 MB file in tmssh and take screenshots a second
apart. Measured while writing this book, tmssh repainted once in six
seconds. Try the idle priority in your own copy.
:::

::: {.box title="Check yourself"}
What does the main loop do on each iteration? Which two things together let
a busy pty share the thread with drawing? Why does the tab close on idle
rather than in the exit callback?
:::
