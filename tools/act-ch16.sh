dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.6; }
dbl 120 143; dbl 120 181; dbl 120 219; dbl 120 105; dbl 120 143
sleep 1; xdotool key ctrl+shift+l; sleep 0.5
xdotool key ctrl+shift+q; sleep 2
