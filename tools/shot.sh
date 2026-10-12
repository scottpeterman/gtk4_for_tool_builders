#!/bin/bash
# usage: WINNAME=<title> shot.sh <binary> <out.png> [actions-script]
# Runs a GTK app on a private Xvfb with openbox, optionally sources an action
# script (with $WID set; it may call `snap <file>` for extra shots), takes a
# screenshot of the window, then closes it politely through the WM.
BIN="$1"; OUT="$2"; ACTIONS="$3"
DISP=:$((90 + RANDOM % 9))
Xvfb $DISP -screen 0 1400x900x24 >/dev/null 2>&1 &
XPID=$!
sleep 1
export DISPLAY=$DISP GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1 GTK_A11Y=none LIBGL_ALWAYS_SOFTWARE=1
openbox >/dev/null 2>&1 &
WMPID=$!
xcompmgr >/dev/null 2>&1 &   # compositor: popovers get real transparency
CMPID=$!
sleep 0.5
"$BIN" > "${OUT%.png}.log" 2>&1 &
APID=$!
WID=""
for i in $(seq 1 40); do
  WID=$(xdotool search --onlyvisible --name "^${WINNAME:-gtklab}" 2>/dev/null | head -1)
  [ -n "$WID" ] && break
  sleep 0.25
done
sleep 1
trimshot() { import -window root "$1" 2>/dev/null && convert "$1" -fuzz 2% -trim +repage "$1"; }
snap() { sleep 0.6; trimshot "$1"; }
if [ -n "$ACTIONS" ] && [ -f "$ACTIONS" ]; then . "$ACTIONS"; fi
sleep 0.7
[ -z "$NOSHOT" ] && trimshot "$OUT"
[ -n "$WID" ] && wmctrl -i -c $WID 2>/dev/null
for i in $(seq 1 20); do kill -0 $APID 2>/dev/null || break; sleep 0.2; done
if kill -0 $APID 2>/dev/null; then kill $APID; echo "(killed: window did not close)" >> "${OUT%.png}.log"; fi
wait $APID 2>/dev/null; echo "exit=$?" >> "${OUT%.png}.log"
kill $CMPID $WMPID $XPID 2>/dev/null
grep -v -e 'libEGL warning' "${OUT%.png}.log"
