#!/bin/bash
# Check one or more chapter files: every include resolves, every image and
# diagram exists. Writes nothing into out/, so it is safe to run in parallel.
# usage: ./check-chapter.sh chapters/210-ch05-listview.md [...]
set -uo pipefail
cd "$(dirname "$0")"
export BOOK_REPO_ROOT="$(cd .. && pwd)"
tmp=$(mktemp -d)
status=0
for f in "$@"; do
  if ! pandoc "$f" --lua-filter=filters/include-code.lua --lua-filter=filters/boxes.lua -t html -o "$tmp/out.html" 2> "$tmp/err"; then
    echo "FAIL $f"; cat "$tmp/err"; status=1; continue
  fi
  for img in $(grep -o '](\(images\|diagrams\)/[^)]*)' "$f" | sed 's/^](//; s/)$//'); do
    [ -f "$img" ] || { echo "MISSING $img in $f"; status=1; }
  done
  echo "ok   $f ($(wc -w < "$f") words)"
done
rm -rf "$tmp"
exit $status
