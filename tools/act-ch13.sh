dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
dbl 120 105; dbl 120 143; sleep 0.8
xdotool type --delay 15 'seq 1 300'; xdotool key Return; sleep 0.8
# scroll back up with the wheel, then select a few lines by dragging
for i in $(seq 1 12); do xdotool mousemove --window $WID 600 300 click 4; done; sleep 0.5
xdotool mousemove --window $WID 400 150 mousedown 1 mousemove --window $WID 450 200 sleep 0.2 mousemove --window $WID 480 240 sleep 0.2 mouseup 1; sleep 0.4
snap ../shots/ch13-scrolled-selected.png
xdotool key ctrl+shift+c; sleep 0.3
# typing jumps to the bottom; paste the selection back
xdotool type --delay 15 'echo pasted: '; xdotool key ctrl+shift+v; sleep 0.4
snap ../shots/ch13-pasted.png
xdotool key ctrl+u; sleep 0.2
# Ctrl+Shift+Q with the grid focused: the capture shortcut quits before the grid sees it
echo "$(date +%H:%M:%S.%3N) ===== pressing ctrl+shift+q" >> /tmp/marks13.log
xdotool key ctrl+shift+q; sleep 1
