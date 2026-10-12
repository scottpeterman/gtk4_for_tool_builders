dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 40 1; sleep 0.15; }
dbl 120 143
for i in $(seq 1 25); do dbl 120 181; dbl 120 219; done
sleep 0.5
xdotool key ctrl+Page_Up ctrl+Page_Up; sleep 0.3
snap ../shots/ckC-50tabs.png
xdotool mousemove --window $WID 745 26 click 1; sleep 0.6
snap ../shots/ckC-menu.png
xdotool key Escape; sleep 0.3
xdotool mousemove --window $WID 600 300 click 3; sleep 0.4
xdotool mousemove --window $WID 620 391 click 1; sleep 0.8   # Close Other Tabs: 49 go
echo "$(date +%H:%M:%S.%3N) ===== after Close Other Tabs" >> /tmp/marksC.log
xdotool key ctrl+shift+w; sleep 0.6; xdotool key Return; sleep 0.6
