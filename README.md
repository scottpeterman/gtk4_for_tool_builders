# gtklab — companion code for "GTK 4 for Tool Builders"

One directory per chapter. Each is the finished state of that chapter and
builds on its own, so a stuck learner can diff their work against it:

    diff -ru ch06-tree/src ch07-actions/src

| Directory | Chapter |
| --- | --- |
| ch01-window | 1  A window that runs |
| ch02-gobject | 2  GObject: references, signals, ownership |
| ch03-layout | 3  Layout and containers |
| ckA-remember | Checkpoint A |
| ch04-core | 4  Keep GTK out of the logic (core library + doctest) |
| ch05-listview | 5  List views I |
| ch06-tree | 6  List views II: trees and filtering |
| ch07-actions | 7  Actions, menus and shortcuts |
| ckB-browser | Checkpoint B |
| ch08-tabs | 8  Tabs that own things |
| ch09-input | 9  Event controllers and propagation |
| ch10-popovers | 10 Popovers and dialogs |
| ckC-tabs | Checkpoint C |
| ch11-mainloop | 11 The main loop |
| ch12-widget | 12 Writing a widget (GtklabGrid) |
| ch13-input | 13 Scrollback, keys and selection |
| ch14-gtkpyte | 14 Swapping in gtkpyte (= Checkpoint D) |
| ch15-resources | 15 Resources, CSS, themes and icons |
| ch16-lifetime | 16 Lifetime, proven |
| ch17-shipping | 17 Holding the floor and packaging |

Build any chapter:

    cd ch07-actions
    meson setup build-debug --buildtype=debug
    meson compile -C build-debug
    ./build-debug/gtklab

Every directory carries the course's `.vscode/` (CodeLLDB, Inspector,
fatal-criticals, gdb; from Chapter 4 also unit tests, a scratch config and an
ASan build).

## Requirements

GTK >= 4.8, meson, ninja, a C++17 compiler. From Chapter 14: network access
once, for meson to fetch utf8proc (subprojects/utf8proc.wrap).

## Licence

Chapters 1-13 are gtklab's own code. Chapters 14-17 vendor gtkpyte
(GPL-3.0-or-later, see gtkpyte/VENDORED.md and gtkpyte/LICENSE-GPL-3.0), so
those builds are GPL-3.0. tests/doctest.h is MIT.

## Tools

- `tools/verify-all.sh` clean-builds every chapter and runs its tests.
- `tools/shot.sh` runs a chapter under Xvfb with an xdotool script and
  screenshots it; `tools/act-*.sh` are the scripts used for the course images.
- `tools/gen_sessions.py N FILE` writes N sessions for list-view exercises.
- `tools/demo_sessions.ini` is the sessions file used in the screenshots.
