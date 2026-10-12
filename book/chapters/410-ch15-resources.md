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

::: {.box title="In this chapter"}
- Compile a stylesheet, an icon and data files into the binary as a
  `GResource` bundle, and read each one back by path.
- Predict which CSS rule wins, from provider priority first and selector
  specificity second.
- Show state with style classes and the theme's named colours, and know
  when a colour belongs in data instead.
- Ship a symbolic icon that GTK finds by name and recolours from CSS.
- Parse colour themes in the core, with tests, and apply them to every open
  terminal from a stateful action.
:::

## Concepts

### A bundle is a file system inside the binary

A **`GResource` bundle** is a set of files compiled into the program and
read back by path, such as `/io/github/gtklab/style.css`. Nothing is
installed beside the binary, so nothing can go missing, be the wrong
version, or be found in the wrong prefix.

It happens in two halves. At build time, an XML manifest lists the files;
Meson's `gnome.compile_resources` runs `glib-compile-resources`, which
packs them into one blob and writes it out as C: a byte array in a section
of its own, and a constructor function. At run time that constructor runs
before `main`, and registers the blob with GLib. From then on any code in the
process can open `/io/github/gtklab/...` as if it were a read-only file
system.

![A bundle at build time and at run time. Three readers take files out of it.](diagrams/ch15-gresource.svg)

Two consequences are worth knowing. The build tracks the files in the
manifest: edit `style.css` and Meson regenerates the C, recompiles it and
relinks gtklab, and nothing else. And the bundle is inspectable in the built
program: `gresource list build-debug/gtklab` prints every path in it, which
is the quickest way to answer "did my file make it in?".

The manifest's `prefix` is the application id written as a path. That is
not only tidy. `GtkApplication` derives a **resource base path** from its
id, `/io/github/gtklab` here, and looks under it for things it uses
automatically: it adds `icons/` beneath it to the icon theme, and loads
`gtk/menus.ui` if there is one. gtklab's `install_style` adds the icon path
again, explicitly; the call is redundant but harmless, and it keeps the
dependency visible in gtklab's own code.

### Three readers

GTK reads two kinds of file straight out of a bundle: a `GtkCssProvider`
loads a stylesheet from a resource path, and the icon theme searches a
resource path for icons. (`GtkBuilder` can also load UI definitions from
one; gtklab builds its UI in code.) Anything else, such as a colour theme,
is just bytes: `g_resources_lookup_data` returns a `GBytes` and your code
parses it. gtklab's theme files take that third route, and the parser is
in the core (Chapter 4), so it is tested without a display.

### The CSS cascade: priority first, then specificity

GTK styles widgets with CSS, matched against each widget's **CSS node**:
its node name (`label`, `image`, `button`), its style classes, its name,
and its state. Rules come from several **style providers**, each added with
a priority:

![Style providers, highest priority first. A property comes from the highest provider that sets it.](diagrams/ch15-cascade.svg){width=60%}

The order of the two rules matters, and it is not the one web CSS teaches.
Priority is decided first: for each property, the highest-priority provider
that sets it wins, however specific a lower provider's selector is.
Specificity only breaks ties inside one provider: an id beats a class, a
class beats a node name, two classes beat one, and between equals the later
rule wins. Measured on GTK 4.8 and 4.14 with two providers of our own: a
`label.error` rule at theme priority lost to a bare `label` rule at
application priority, on a label with the `error` class.

That is why `style.css` uses narrow selectors (`image.log-indicator`,
`label.error.banner`). gtklab's stylesheet sits at
`GTK_STYLE_PROVIDER_PRIORITY_APPLICATION`, above the theme; a broad rule
there such as `label { color: ... }` would override the theme on every label
in the program, error labels and dim labels included. And it sits below
`USER`, so the user's own `gtk.css` still has the last word.

### Classes, named colours, and colours in data

There are four places a colour can come from, and they do not behave the
same when the user switches to dark or high contrast:

| Where the colour comes from | Follows light and dark | Use it for |
|---|---|---|
| A theme class: `error`, `dim-label`, `caption` | yes | common states the theme already styles |
| Your class, a named colour: `@error_color` | yes | your own element in a theme colour |
| Your class, a literal: `#c01c28` | no | almost nothing |
| A widget API, from data: `apply_theme` | by choice | content colours the user picks |

The first two are the idiom: code decides *what state a widget is in*, by
adding or removing a class; CSS decides *what that looks like*. The
sessions-file banner from Checkpoint B does exactly this, with `error` from the
theme and `banner` from `style.css`, whose background is
`alpha(@error_color, 0.12)`: a tint of whatever red the current theme uses.

The terminal palette is the exception, and deliberately so. Sixteen ANSI
colours, a background and a cursor are content the user chooses, not
chrome, and a terminal theme is independent of the application's light or
dark variant. They are data: parsed in the core, handed to gtkpyte through
its own API, which queues a redraw (Chapter 12).

### Symbolic icons

A **symbolic** icon is a single-colour SVG whose name ends in `-symbolic`.
GTK does not draw its colours as written. It renders the SVG with a
stylesheet of its own that forces every `rect`, `circle` and `path` to the
widget's CSS `color`; shapes with the class `warning`, `error` or `success`
get the theme's colours for those. gtklab's log dot is a circle filled
`#222222` in the file, and it is drawn in `@error_color` because
`style.css` sets `color` on `image.log-indicator`. Change the rule and the
icon follows; switch the application to dark and it follows the dark red.

### How GTK finds an icon by name

`gtk_image_new_from_icon_name("gtklab-log-symbolic")` names an icon; it
does not name a file. The icon theme searches for it:

![Icon lookup. Resource paths are searched as part of hicolor, the last theme in the chain.](diagrams/ch15-icon-lookup.svg)

The user's theme (Adwaita, say) comes first, then the themes it inherits,
then **hicolor**, the fallback every theme ends in. Resource paths are
treated as part of hicolor, and must use its layout:
`<path>/scalable/actions/name.svg` or `<path>/16x16/actions/name.png`. Two
consequences follow. A system icon with the same name wins over yours,
which is why gtklab prefixes its icon names with `gtklab-`. And a name that
is found nowhere is not an error: measured, the lookup returns GTK's
`image-missing` icon with no warning at all. `gtk_icon_theme_has_icon`
answers the question directly when you need to know.

### Where this sits in the book

The theme parser is Chapter 4's boundary again: GTK only reads bytes from
the bundle and applies colours; everything that can be wrong with a theme
file is decided, and tested, in the core. The theme menu is Chapter 7's
stateful radio action with a string state. And the application's light or
dark variant is still Chapter 7's `gtk-application-prefer-dark-theme`; the
terminal theme is a second, independent setting.

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

## Review

1. gtklab has no installed data files at all. What does the program contain
   that makes `/io/github/gtklab/style.css` readable, and when is it
   registered?
2. You edit `nord.ini` and rebuild. Which build steps run, and why does the
   core library not need to be recompiled?
3. A rule `label.error { color: blue; }` at theme priority and a rule
   `label { color: red; }` at application priority both match an error
   label. Which colour does it get, and why does that argue for narrow
   selectors in `style.css`?
4. Why is the terminal palette passed through `apply_theme` rather than
   written as CSS like the rest of gtklab's colours?
5. The log icon's SVG fills its circle with `#222222`. Why does it draw in
   red, and what would make it draw in green?
6. You rename the icon file to `gtklab-log.svg`, keeping it in the bundle,
   and change the name in code to match. What changes on screen, and why?
7. A system icon theme happens to ship an icon called `log-symbolic`. What
   happens if gtklab bundles its own icon under that name?
8. Why can a theme file's bad colour be reported with its key name, without
   any GTK running?

**Questions for deeper thought.** A tool you write is used by people with
high-contrast themes and large fonts. Which of your colours would you let
the theme decide, and which are genuinely data the user owns? If you
bundled thirty terminal themes, as tmssh does, would you still read them
from the bundle at start-up, or let users add their own from a directory,
and what would the error path look like for a bad user file? What is lost
when everything is in the binary?

## Exercises

1. **A fifth theme.** In `ch15-resources`, add `data/themes/gruvbox.ini`
   (any twenty valid colours) and list it in `data/gtklab.gresource.xml`.
   Done when `gresource list build-debug/gtklab` shows it and it appears in
   the Terminal Theme submenu in its sorted place, with no change to
   `load_themes`.
2. **A broken theme, reported.** Give `gruvbox.ini` a colour of `#12345`.
   Done when gtklab starts with the other four themes and the banner names
   the file and the key, and a new case in `tests/test_theme.cpp` proves
   `parse_theme` rejects a five-digit colour.
3. **A user stylesheet.** In `AppWindow::install_style`, also load
   `style.css` from gtklab's configuration directory (`config_path` gives
   the path) when it exists, at `GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1`.
   Done when `image.log-indicator { color: green; }` in that file turns the
   log dot green, a missing file is silent, and a syntax error reaches the
   banner through the provider's `parsing-error` signal.
4. **A warning, not an error.** Theme-file problems are not fatal; show
   them in the banner with the class `warning` instead of `error`, and add
   a `label.warning.banner` rule to `style.css` using `@warning_color`.
   Done when a bad theme file shows an amber banner, a bad sessions file
   still shows a red one, and no colour literal appears in C++.
