dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.4; }
snap ../shots/ch06-start.png
# rows: Local, Core, Edge, DC, Lab at ~38px pitch starting y~105
dbl 120 143   # Core
snap ../shots/ch06-core-open.png
xdotool mousemove --window $WID 130 72 click 1; xdotool type --delay 60 "fra"; sleep 0.8
snap ../shots/ch06-filtered.png
xdotool key ctrl+a BackSpace; sleep 0.8
snap ../shots/ch06-cleared.png
dbl 120 181   # first session under Core
