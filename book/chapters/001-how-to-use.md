# How to use this book {.unnumbered}

## One application, built in steps {.unnumbered}

You build gtklab in seventeen chapters. Each chapter adds one idea, and its
code compiles and runs when you finish it. Four checkpoints close the parts
of the book; they give you a specification and a definition of done, and no
step-by-step instructions, so you can find out what you have actually
learned.

The companion repository has one directory per chapter, each holding the
finished state of that chapter:

| Directory | Chapter |
| --- | --- |
| `ch01-window` … `ch03-layout` | Chapters 1 to 3 |
| `ckA-remember` | Checkpoint A |
| `ch04-core` … `ch07-actions` | Chapters 4 to 7 |
| `ckB-browser` | Checkpoint B |
| `ch08-tabs` … `ch10-popovers` | Chapters 8 to 10 |
| `ckC-tabs` | Checkpoint C |
| `ch11-mainloop` … `ch14-gtkpyte` | Chapters 11 to 14 (Chapter 14 is also Checkpoint D) |
| `ch15-resources` … `ch17-shipping` | Chapters 15 to 17 |

Work in your own repository and use these directories to check your work.
When you are stuck, compare your source with the reference for the chapter
you are on; when you are done, compare it with the next one to see the size
of the step ahead.

```
diff -ru ch06-tree/src ch07-actions/src
```

## Two tracks in every chapter {.unnumbered}

**Build.** You write the chapter's increment in your own copy of gtklab. It
compiles and runs at the end of the chapter.

**Read.** You then read how tmssh solved the same problem, by function name.
In VS Code, Ctrl+T (Cmd+T on the Mac) finds a function by name. You read
after you build, so that you are comparing, not copying.

## The parts of a chapter {.unnumbered}

Every chapter has the same parts, in the same order:

1. **Recall.** Two or three questions on earlier chapters, answered before
   you read on. The answers are in the appendix.
2. **The idea.** One concept, explained far enough to use it.
3. **Build.** The increment, with the code.
4. **Done when.** What the running program must do, and a screenshot of it
   doing it.
5. **Debugger drill.** A breakpoint exercise that makes the idea visible.
6. **Break it.** A deliberate bug, and exactly what GTK does about it: a
   critical warning, a crash, or nothing at all.
7. **Read in tmssh.** Where the production version lives.

Most chapters also have a short **Common mistakes** section, collected from
the mistakes made while the book's code was written and tested.

## Four ideas that keep coming back {.unnumbered}

Four ideas cause most real GTK bugs, so the book does not teach them once and
move on. Each is introduced early and comes back in later chapters, and the
Recall questions are drawn from them, spaced so that each returns after a
short gap, then a longer one.

| Strand | Introduced | Comes back in | Tested in |
| --- | --- | --- | --- |
| Lifetime and ownership | Chapter 2 | 5, 8, 10, 12 | Chapter 16 |
| The main loop | Chapter 1 | 7, 10, 11 | Chapter 14 |
| Model and widget | Chapter 4 | 5, 6, 12 | Chapter 15 |
| Input propagation | Chapter 9 | 10, 13 | Chapter 14 |

## Conventions {.unnumbered}

Code blocks in this book are taken from the repository when the book is
built. A small grey line above each block names the file it came from.
Shell commands are written one per line. Shortcuts are written as they are
pressed on Linux; on the Mac, gtklab uses the same Ctrl+Shift shortcuts.

Function names in the text refer to the code, not to line numbers, so that
they survive edits. The debug output quoted in the book comes from
`g_debug` lines in the code, which print when the program runs with
`G_MESSAGES_DEBUG=gtklab`.
