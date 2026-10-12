#!/bin/bash
# Clean-build every chapter, run its unit tests, and report warnings.
# usage: tools/verify-all.sh            (from the repo root)
set -u
cd "$(dirname "$0")/.."
status=0
for d in ch01-window ch02-gobject ch03-layout ckA-remember ch04-core ch05-listview ch06-tree \
         ch07-actions ckB-browser ch08-tabs ch09-input ch10-popovers ckC-tabs ch11-mainloop \
         ch12-widget ch13-input ch14-gtkpyte ch15-resources ch16-lifetime ch17-shipping; do
  rm -rf "$d/build-verify"
  if ! meson setup "$d/build-verify" "$d" --buildtype=debug > "/tmp/verify-$d.log" 2>&1; then
    echo "FAIL setup  $d"; status=1; continue
  fi
  if ! meson compile -C "$d/build-verify" >> "/tmp/verify-$d.log" 2>&1; then
    echo "FAIL build  $d"; status=1; continue
  fi
  warnings=$(grep -c 'warning:' "/tmp/verify-$d.log" | tr -d ' ')
  own=$(grep 'warning:' "/tmp/verify-$d.log" | grep -vc 'gtkpyte/' | tr -d ' ')
  tests="-"
  if [ -x "$d/build-verify/gtklab-tests" ]; then
    if meson test -C "$d/build-verify" >> "/tmp/verify-$d.log" 2>&1; then tests="pass"; else tests="FAIL"; status=1; fi
  fi
  printf 'ok    %-16s warnings(own/all)=%s/%s tests=%s\n' "$d" "$own" "$warnings" "$tests"
  rm -rf "$d/build-verify"
done
exit $status
