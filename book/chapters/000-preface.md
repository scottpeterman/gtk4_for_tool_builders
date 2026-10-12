# Preface {.unnumbered}

This is a book about building desktop tools with GTK 4: the small, sharp
programs that engineers write for themselves and their teams. A session
manager, a log viewer, a terminal, a dashboard that sits on a second monitor.
It is written for people who already program for a living and want to add a
native toolkit to what they can build, not for people learning to program.

The book follows one application from an empty window to a finished, packaged
tool. That application is **gtklab**, a small session-manager terminal: a tree
of saved sessions on the left, tabs of live terminals on the right, menus,
shortcuts, themes and logging. gtklab is a teaching copy of **tmssh**, a GTK 4
terminal that is in daily use, and each chapter closes by pointing at the
place in tmssh where the same problem was solved for real.

## Why GTK 4, and why this way {.unnumbered}

GTK 4 is the toolkit underneath most of the Linux desktop, and it is a good
fit for tools. It starts fast, it is small, it renders with the GPU, and its
C API can be used from any language that can call C. It is also a large and
old codebase with its own object system, GObject, and a set of conventions
that are rarely written down in one place. Most GTK material online is for
GTK 3, and much of it no longer applies.

The difficulty for an experienced developer is rarely the widgets. It is the
machinery around them: who owns which object, when a callback can run, why a
handler fires on an object that is already being destroyed, why the window
stops repainting while a terminal prints. Those are the subjects this book
spends its time on. They come back again and again, chapter after chapter,
because that is how they are learned.

## How this book was made {.unnumbered}

Every example in this book is real code from the companion repository, and
every chapter's code builds and runs. The code blocks are not copied into the
manuscript by hand: the build pulls them from the repository's chapter
directories, so the book cannot show code that the repository does not
contain.

Each chapter's code was built from scratch with no compiler warnings on
Ubuntu 24.04 with GTK 4.14.5, its unit tests run, and the running program was
driven by scripts that click, type and take the screenshots you will see.
Every "Break it" exercise was carried out and its result recorded. The
finished application was also built and run against GTK 4.8.3, the oldest
version the book supports, so that it works on Debian 12 and on ChromeOS
Linux.

Where testing found that a common piece of GTK advice was wrong, or that the
first version of a chapter had a bug, the book says so. Several of the most
useful lessons in it came from exactly that.

## What you need {.unnumbered}

- A Linux machine with GTK 4.8 or later, or a Mac with GTK 4 from Homebrew.
  ChromeOS Linux (Crostini) works.
- C++17, and enough C to read a C API. You do not need to know GObject; the
  book teaches it as it is needed.
- VS Code, which the book uses for building and debugging. Any editor and
  debugger will do, but the launch configurations are written for VS Code.
- The companion repository: <https://github.com/scottpeterman/gtk4_for_tool_builders>.

## Licences {.unnumbered}

The text of this book is licensed under Creative Commons
Attribution-ShareAlike 4.0 (CC BY-SA 4.0). The code in the companion
repository is licensed under the GNU General Public License, version 3 or
later. From Chapter 14, gtklab includes gtkpyte, which is GPL-3.0-or-later;
`tests/doctest.h` is MIT.
