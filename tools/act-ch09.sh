mark() { echo "$(date +%H:%M:%S.%3N) ===== $1" >> /tmp/marks9.log; sleep 0.15; }
dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
dbl 120 143; dbl 120 181
mark "1 left-click"; xdotool mousemove --window $WID 500 300 click 1; sleep 0.4
mark "2 right-click"; xdotool mousemove --window $WID 500 300 click 3; sleep 0.4
mark "3 wheel"; xdotool mousemove --window $WID 500 300 click 5; sleep 0.4
mark "4 ctrl+wheel"; xdotool keydown ctrl; xdotool mousemove --window $WID 500 300 click 4 click 4; xdotool keyup ctrl; sleep 0.4
mark "end"
snap ../shots/ch09-zoom.png
