dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
dbl 120 105; dbl 120 143; sleep 0.6
xdotool type --delay 20 'stty size; printf "caf\xc3\xa9 \xe2\x86\x92 -> == != tabs:\t|\t|\n"; ls /'; xdotool key Return; sleep 0.8
snap ../shots/ch12-grid.png
xdotool windowsize $WID 1100 700; sleep 0.8
xdotool type --delay 20 'stty size'; xdotool key Return; sleep 0.6
snap ../shots/ch12-resized.png
