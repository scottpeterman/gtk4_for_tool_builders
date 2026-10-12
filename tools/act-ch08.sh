dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
dbl 120 143      # expand Core
dbl 120 181      # core1-den
dbl 120 219      # core2-den
dbl 120 105      # Local folder
dbl 120 143      # localhost
snap ../shots/ch08-tabs.png
xdotool mousemove --window $WID 367 65 click 1; sleep 0.5
snap ../shots/ch08-closed.png
