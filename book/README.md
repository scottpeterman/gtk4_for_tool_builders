# GTK 4 for Tool Builders — book source

The text of the book, built from Markdown with pandoc. Every code block is
pulled from the chapter directories one level up when the book is built, so
the book cannot drift from code that compiles.

## Build

Needs pandoc 3, xelatex with KOMA-Script, fvextra and tcolorbox, rsvg-convert,
the TeX Gyre Pagella and IBM Plex fonts, and Graphviz to regenerate diagrams.
On Debian or Ubuntu:

```
sudo apt install pandoc texlive-xetex texlive-latex-extra texlive-fonts-recommended fonts-texgyre fonts-ibm-plex librsvg2-bin graphviz
```

Then, from this directory:

```
./build.sh all
```

| Command | Output |
| --- | --- |
| `./build.sh pdf` | `out/gtk4-for-tool-builders.pdf` |
| `./build.sh html` | `out/html/`, one page per chapter, ready for GitHub Pages |
| `./build.sh md` | `out/gtk4-for-tool-builders.md`, a single GitHub-flavoured Markdown file |

## Layout

| Path | What |
| --- | --- |
| `chapters/` | One file per chapter, in reading order by file name |
| `filters/include-code.lua` | Fills `include="dir/file"` code blocks from the repository; `fn=`, `from=`/`to=` select an excerpt. A missing file or anchor fails the build |
| `filters/boxes.lua` | `::: {.box title="..."}` blocks and part headings |
| `images/` | Screenshots, all taken from the chapter code under Xvfb |
| `diagrams/` | SVG diagrams; sources in `diagrams/src/*.dot`, regenerate with `diagrams/render.sh` |
| `templates/` | LaTeX header and the HTML stylesheet |

## Licence

The text, images and diagrams of the book are licensed under
[Creative Commons Attribution-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-sa/4.0/)
(CC BY-SA 4.0); see `LICENSE`. The code, in the chapter directories and as
quoted in the book, is GPL-3.0-or-later, like the rest of the repository.
