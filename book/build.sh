#!/bin/bash
# Build the book: PDF, HTML (one page per chapter) and a single Markdown file.
#
# usage: book/build.sh [pdf|html|md|all]     (default: all)
#
# Needs pandoc >= 3, xelatex with KOMA-Script, fvextra and tcolorbox,
# rsvg-convert (for SVG figures in the PDF), the TeX Gyre Pagella and IBM Plex
# fonts. Code blocks are filled from the chapter directories at build time by
# filters/include-code.lua, so the book always shows the code that builds.
set -euo pipefail
cd "$(dirname "$0")"
export BOOK_REPO_ROOT="$(cd .. && pwd)"
WHAT="${1:-all}"
mkdir -p out
# Appendix F is assembled from answers/chNN.md, one file per chapter.
{
  echo "# Appendix F — Review answers"
  echo
  echo "Short answers to each chapter's Review questions. Check your own answer"
  echo "against the chapter first; these say what a good answer must contain."
  for f in answers/ch*.md; do echo; cat "$f"; done
} > chapters/510-review-answers.md
CHAPTERS=$(ls chapters/*.md | sort)
COMMON=(metadata.yaml $CHAPTERS --lua-filter=filters/include-code.lua --lua-filter=filters/boxes.lua --resource-path=.:images:diagrams)

if [ "$WHAT" = all ] || [ "$WHAT" = pdf ]; then
  pandoc "${COMMON[@]}" --pdf-engine=xelatex -H templates/latex-header.tex -o out/gtk4-for-tool-builders.pdf
  echo "out/gtk4-for-tool-builders.pdf"
fi

if [ "$WHAT" = all ] || [ "$WHAT" = html ]; then
  rm -rf out/html
  pandoc "${COMMON[@]}" --to=chunkedhtml --split-level=1 --css=book.css --standalone --toc -o out/html
  cp templates/book.css out/html/
  mkdir -p out/html/images out/html/diagrams
  cp images/* out/html/images/ 2>/dev/null || true
  cp diagrams/*.svg out/html/diagrams/ 2>/dev/null || true
  echo "out/html/index.html"
fi

if [ "$WHAT" = all ] || [ "$WHAT" = md ]; then
  pandoc "${COMMON[@]}" --to=gfm --standalone -o out/gtk4-for-tool-builders.md
  echo "out/gtk4-for-tool-builders.md"
fi
