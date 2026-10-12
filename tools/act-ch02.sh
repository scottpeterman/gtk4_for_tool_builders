click() { xdotool mousemove --window $WID $1 $2 click 1; sleep 0.3; }
click 450 25; click 450 25; click 450 25
snap ../shots/ch02-counting.png
click 450 60; sleep 0.3
snap ../shots/ch02-removed.png
click 450 25; sleep 0.3
