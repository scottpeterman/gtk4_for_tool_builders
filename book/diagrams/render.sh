#!/bin/bash
# Regenerate every diagram from diagrams/src/*.dot. The DOCTYPE line Graphviz
# writes is dropped: some hosts refuse SVG files that declare a DTD.
set -euo pipefail
cd "$(dirname "$0")"
for f in src/*.dot; do
  out="$(basename "${f%.dot}").svg"
  dot -Tsvg "$f" | python3 -c 'import re,sys; sys.stdout.write(re.sub(r"<!DOCTYPE[^>]*>\s*", "", sys.stdin.read(), flags=re.S))' > "$out"
done
