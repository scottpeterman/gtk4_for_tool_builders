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

::: {.box title="In this chapter"}
- Trace one iteration of the GLib main loop and predict which ready source
  it dispatches.
- Watch a file descriptor at a priority that leaves room for drawing, and
  bound the work each callback does.
- Run a child on a pseudo-terminal and explain what the kernel does between
  your bytes and the shell's.
- Remove every source that points at an object before the object goes, and
  defer work that would delete the caller.
- Turn a burst of file-change events into one reload.
:::

## Concepts

### One thread, many sources

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

Signals are not on the list because they are not sources. A
`GFileMonitor`'s `changed` signal or a button's `clicked` is a function call
made from inside some source's callback, on the same thread, before that
callback returns.

The single thread is a rule, not a choice: GTK widgets may only be touched
from the thread that runs the main loop. You could read the pty on a worker
thread, but every byte would then have to be handed back to the main thread
before it reached a widget. A non-blocking descriptor watched by the main
loop gets the same result with no locks and no hand-off.

### One iteration

`g_main_context_iteration` runs the same five steps every time:

![One iteration of the GLib main loop. Only the sources at the highest ready priority are dispatched; the rest wait for a later iteration.](diagrams/ch11-iteration.svg)

1. **Prepare.** Each source is asked whether it is ready without waiting.
   An idle always is; a timer is if its time has passed; a descriptor
   cannot know yet. GLib notes the highest priority among the ready ones.
2. **Query.** GLib collects the descriptors to poll and a timeout: zero if
   something is already ready, the nearest timer otherwise, forever if
   nothing is pending.
3. **Poll.** The thread sleeps in `poll()` until a descriptor is ready or the
   timeout expires. An idle program spends nearly all its time here, using
   no CPU.
4. **Check.** Each source is asked again, now that the poll has answered.
5. **Dispatch.** The callbacks of the ready sources at the winning priority
   run, one after another. Each returns `G_SOURCE_CONTINUE` to stay or
   `G_SOURCE_REMOVE` to be destroyed.

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
urgent is ready. A ten-line test program shows it plainly: with an idle at
priority 0 that stays ready for 1,000 iterations and an idle at the default
idle priority beside it, the second one first ran after the first had run
1,000 times and removed itself.

### Priorities and the redraw

![GLib priorities, most urgent at the top. A source that is always ready at the default priority wins every iteration, and GTK's redraw, below it, never gets a turn.](diagrams/priorities.svg)

The numbers that matter, from the GLib and GDK headers and from asking each
source for its priority with `g_source_get_priority`:

| Source | Added with | Priority | gtklab uses it for |
| --- | --- | --- | --- |
| Window system events | (GTK) | 0, `GDK_PRIORITY_EVENTS` | clicks, keys |
| Timer | `g_timeout_add` | 0 | debounced save (500 ms), sessions reload (300 ms) |
| Descriptor | `g_unix_fd_add` | 0 | (not used: see below) |
| Child exit | `g_child_watch_add` | 0 | closing the tab |
| GTK's frame | (GTK) | 120, `GDK_PRIORITY_REDRAW` | layout and paint |
| Idle | `g_idle_add` | 200, `G_PRIORITY_DEFAULT_IDLE` | focus, deferred close, popover cleanup |
| Descriptor, chosen | `g_unix_fd_add_full` | 200 | reading the pty |

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

The budget and the priority do different jobs, and you need both. The
budget bounds how long one callback holds the thread; the priority decides
who gets the thread next.

### A source points at you; you own the source

A source holds a function and a plain pointer, usually `this`. It takes no
reference and has no idea when the object goes away. So the object must
remove its sources before it goes, by the id each `g_*_add` returned, and
must forget an id once the source has removed itself by returning
`G_SOURCE_REMOVE`. That is the whole of `~PtySession`'s ordering rule, and
the same rule behind the `save_source_` id of Chapter 7's debounced save.

The other half is the **deferred call**. Some work cannot happen inside the
callback that wants it: closing the tab from the child-exit callback would
delete the session whose callback is still running; unparenting a popover
from its own `closed` handler (Chapter 10) would destroy it mid-signal;
grabbing focus while a click is still being handled is undone when the
click finishes. Each of these posts an idle that does the work after the
current dispatch returns. An idle that touches a widget holds a reference to
it, so it cannot vanish in between.

A **debounce** is the timer version of the same idea. Several events arrive
close together; each one removes the pending timer and starts a new one; only
the last one survives to fire.

### A pseudo-terminal

A terminal program runs its child on a **pseudo-terminal** (pty): a pair of
devices that behave like the two ends of a serial line. The child's end, the
**slave** (`/dev/pts/N`), looks like a real terminal to it, so shells print
prompts and programs like `vim` and `htop` draw full-screen. Your end, the
**master**, is a file descriptor: you read what the child writes and write
what the user types.

![A pty. Between your descriptor and the child sits the kernel's line discipline, which echoes, translates line endings and turns control characters into signals.](diagrams/ch11-pty.svg)

Between the two ends sits the kernel's **line discipline**, and it does more
than pass bytes along. With the default settings of a new pty, writing
`hi\r` to the master delivered `hi\n` to the program on the slave (carriage
return becomes newline on input) and `hi\r\n` came back out of the master
(the input is echoed, and newline becomes carriage return plus newline on
output). It also turns byte 3, Ctrl+C, into `SIGINT` for the program in the
foreground. That is why a terminal never draws what you type and never sends
signals: it sends bytes, and the echo and the signals come from the kernel
or the shell.

`forkpty` does the setup in one call: it opens a pty, forks, and in the
child starts a new session, makes the slave its controlling terminal, and
makes it standard input, output and error. The parent gets the child's pid
and the master descriptor. Window size travels separately: the `TIOCSWINSZ`
ioctl on the master sets the size, and the kernel sends `SIGWINCH` to the
foreground process group. A test child that printed its terminal size from a
`SIGWINCH` handler reported 100 columns by 30 rows right after the parent set
them.

When the child exits and the last copy of the slave closes, reading the
master returns 0, or fails with `EIO` on Linux. That is the end of output;
the exit status comes separately, from the child watch.

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

## Review

1. In one iteration, a timer added with `g_timeout_add` is due, a descriptor
   watched with `g_unix_fd_add` is readable, and an idle added with
   `g_idle_add` is pending. Which callbacks run in this iteration, and which
   waits?
2. `on_readable` already stops after 512 KiB. Why is that not enough to keep
   the window repainting during `cat` of a large file at the default
   priority?
3. A program has one idle that always returns `G_SOURCE_CONTINUE`; another
   has only descriptor watches. Why does the first keep a CPU core busy
   while the second uses none until data arrives?
4. The input line sends `ls\n` to the session, and `ls` appears in the
   output view before the listing. gtklab never wrote it there. Where did it
   come from?
5. Byte 3 written to the pty stops `sleep 100` in the tab. Which part of the
   system turns that byte into a signal, and which processes receive it?
6. The exit handler closes the tab from an idle that holds a reference to the
   page. What goes wrong without the idle? Without the reference?
7. `on_readable` sets `read_watch_ = 0` before returning `G_SOURCE_REMOVE`.
   What would happen later if it did not?
8. Why does the sessions file reload restart its timer on every event,
   rather than reloading on the first event and ignoring the rest?

**Questions for deeper thought.** tmssh collects the exit status only after
end of file on the pty. What does your own tool lose if "child exited"
arrives before "output drained"? A tool
that polls two hundred devices over ssh has two hundred descriptors at the
same priority: what does the read budget do for fairness between them, and
what does it not do? And which work in your tools would still belong on a
worker thread, with only its result handed to the main loop?

## Exercises

1. **Count the wakeups.** In `ch11-mainloop/src/pty_session.cpp`, give
   `PtySession` two counters, wakeups and bytes, updated in
   `PtySession::on_readable`, and print them with `g_debug` in
   `~PtySession`. Done when running `seq 3000000` and then closing the tab
   logs both numbers, at most `kReadBudget` plus one 16 KiB read per wakeup
   (the drill's log says 528255 bytes, not 524288).
2. **Change the budget.** With exercise 1 in place, set `kReadBudget` to
   16 KiB and run `time cat` on a large file in a tab, then do the same at
   512 KiB. Done when you have the wakeup count and the total time for both
   budgets written down.
3. **Keep a failed tab open.** In `AppWindow::start_tab`
   (`ch11-mainloop/src/app_window.cpp`), close the tab on idle only when
   the child exited with status 0; otherwise append
   `[exited with status N]` to the view and leave the tab open. Use
   `g_spawn_check_wait_status` to read the status. Done when `exit 3`
   leaves the tab with the message and `exit` closes it.
4. **Name the events.** In `AppWindow::on_sessions_changed`, log each event
   by name with `g_enum_to_string(G_TYPE_FILE_MONITOR_EVENT, event)`. Done
   when one save from your editor lists every event it caused, followed by
   exactly one `sessions file settled; reloading` line.

