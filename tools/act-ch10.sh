dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
dbl 120 143; dbl 120 181; dbl 120 219
xdotool mousemove --window $WID 520 260 click 3; sleep 0.6
snap ../shots/ch10-terminal-menu.png
xdotool key Escape; sleep 0.4
xdotool mousemove --window $WID 110 181 click 3; sleep 0.6
snap ../shots/ch10-row-menu.png
xdotool key Escape; sleep 0.4
xdotool key ctrl+shift+w; sleep 0.8
snap ../shots/ch10-confirm.png
xdotool key Return; sleep 0.6
snap ../shots/ch10-after.png
