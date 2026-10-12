# Chapter 15 — Resources, CSS, themes and icons

Until now gtklab's colours have been written in code and its only icons have
come from the system theme. This chapter adds four terminal colour themes, a
stylesheet and an icon of its own, all compiled into the binary so that the
program needs nothing installed beside it, and a menu that picks a terminal
theme independently of the light or dark application theme.

::: {.box title="Recall"}
What does `gtk_widget_queue_draw` do, and what does it not do (Chapter 12)?
What goes in the core library, and what does not (Chapter 4)? Which gtkpyte
calls set colours (its header, Chapter 14)?
:::

## The idea: resources and style classes

A **`GResource`** bundle is a set of files compiled into the program and
read back by path, such as `/io/github/gtklab/style.css`. An XML manifest
lists the files; Meson's `gnome.compile_resources` turns it into C source
that registers the bundle when the program starts. Nothing is installed, so
nothing can go missing.

GTK reads three kinds of things straight out of a bundle: CSS stylesheets,
icons (once their path is added to the icon theme) and UI definitions. Any
other file, such as a colour theme, you read with
`g_resources_lookup_data` and parse yourself.

**CSS in GTK** styles widgets by name and by style class: `label.error`
matches labels with the `error` class. The system theme already defines the
common classes (`error`, `dim-label`, `title`, `caption`), so the
idiomatic way to show state is to add or remove a class and let the theme
decide what it looks like, rather than setting colours in code. A
**symbolic** icon is a single-colour SVG that takes its colour from CSS, so
the same icon is red under the `error` class and grey elsewhere.

## Build

### The bundle

```{.xml include="ch15-resources/data/gtklab.gresource.xml"}
```

`gnome.compile_resources` generates C, so the project must now list C as a
language. Without it, configuring fails with
`No host machine compiler for 'gtklab-resources.c'`:

```{.meson include="ch15-resources/meson.build" from="project('gtklab'" to="default_options:"}
```

```{.meson include="ch15-resources/meson.build" from="# Files compiled into the binary" to="source_dir: 'data')"}
```

The generated source goes into the executable's sources with
`src/theme_apply.cpp`; `src/core/theme.cpp` joins the core, and
`tests/test_theme.cpp` the tests.

### Themes: parsed in the core

A theme file is a key file with a label, a type, and twenty colours:

```{.ini include="ch15-resources/data/themes/nord.ini"}
```

Parsing it needs no GTK, so it lives in the core with tests, and rejects a
bad colour by naming the key:

```{.cpp include="ch15-resources/src/core/theme.h"}
```

```{.cpp include="ch15-resources/tests/test_theme.cpp" fn="TEST_CASE(\"theme: bad colours are rejected with the key named\")"}
```

### Themes: read from the bundle, applied to terminals

Only reading the bundle and applying colours need GTK. `load_themes` lists
the bundle's theme directory, parses each file and reports any that fail:

```{.cpp include="ch15-resources/src/theme_apply.cpp" fn="std::vector<Theme> load_themes("}
```

`apply_theme` hands the colours to gtkpyte, the selection with some
transparency because it is painted over the text:

```{.cpp include="ch15-resources/src/theme_apply.cpp" fn="void apply_theme("}
```

### Style and icons

The stylesheet uses the theme's named colours, so it works in light and dark:

```{.css include="ch15-resources/data/style.css"}
```

`install_style` loads it for the whole display, and adds the bundle's icon
directory to the icon theme. GTK then finds
`icons/scalable/actions/gtklab-log-symbolic.svg` by the name
`gtklab-log-symbolic`, exactly like a system icon:

```{.cpp include="ch15-resources/src/app_window.cpp" fn="void AppWindow::install_style("}
```

The log dot from Chapter 14 becomes that icon, with the `log-indicator`
class.

### The theme menu

The Terminal Theme submenu is built from the loaded themes, one radio item
per theme on a `win.terminal-theme` action with a string state, the same
pattern as Chapter 7's light and dark choice:

```{.cpp include="ch15-resources/src/app_window.cpp" from="// The theme list is data" to="g_object_unref(theme_menu);"}
```

```{.cpp include="ch15-resources/src/app_window.cpp" fn="void AppWindow::act_terminal_theme("}
```

The application's light and dark variant (Chapter 7) and the terminal theme
are independent settings; a dark application with a light terminal is a
legitimate choice.

## Done when

Choosing a terminal theme recolours every open tab, the application's light
or dark choice is independent of it, and the log icon renders. The core tests
pass, now with the theme tests.

![gtklab Dark; Nord; and a dark application with the light terminal theme. The log icon is in each tab label.](images/ch15-themes.png)

::: {.box title="Debugger drill"}
Run **gtklab + GTK Inspector**. On the CSS tab, type
`image.log-indicator { color: #00c000; }` and watch the log dot turn green as
you type. Then open the Resources tab and find `/io/github/gtklab`, with the
four themes, the stylesheet and the icon inside it.
:::

::: {.box title="Break it"}
In `theme_apply.cpp`, misspell the theme directory as
`/io/github/gtklab/theme`. `g_resources_enumerate_children` returns `NULL`
and sets its `GError`. gtklab puts the message in the sidebar's error banner
rather than starting with no themes and no explanation.
:::

![The Break it: the resource error, shown rather than swallowed.](images/ch15-banner.png){width=35%}

## Common mistakes

**A project without C.** `compile_resources` output is C; list it in
`project()`.

**Colours in code.** They ignore the dark theme and the user's accessibility
settings. Add a class; let CSS decide.

**Forgetting `-symbolic`.** Only icons whose names end in `-symbolic` are
recoloured by CSS.

**Icons at the wrong path.** An icon theme path expects the theme layout
underneath it, `scalable/actions/` and the like. An icon in the wrong
directory is silently not found.

::: {.box title="Read in tmssh"}
`data/tmssh.gresource.xml`, which bundles thirty YAML terminal themes;
`load_themes` in `src/theme_apply.cpp`; `set_app_variant`; and
`release_appimage_theme_pin`, which drops an AppImage's bundled `GTK_THEME`
so that switching between light and dark works there. tmssh bundles themes
only; its icons come from the system.
:::

::: {.box title="Check yourself"}
What is in the bundle, and how is each kind of file read back? Why is the
theme parser in the core? Why does gtklab show state with style classes
rather than colours?
:::
