dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
xdotool windowsize $WID 1000 760; sleep 0.6
dbl 120 105; dbl 120 143; sleep 1
xdotool key ctrl+shift+l; sleep 0.3
xdotool type --delay 10 'ls --color=always /usr/share | head -12; printf "\e[31mred \e[32mgreen \e[33myellow \e[34mblue \e[35mmagenta \e[36mcyan\e[0m\n"'; xdotool key Return; sleep 0.8
snap ../shots/ch15-dark.png
xdotool mousemove --window $WID 845 26 click 1; sleep 0.6
xdotool mousemove --window $WID 760 561 click 1; sleep 0.6
snap ../shots/ch15-themes.png
xdotool mousemove --window $WID 760 162 click 1; sleep 0.8
snap ../shots/ch15-nord.png
# light app + light terminal
xdotool mousemove --window $WID 845 26 click 1; sleep 0.6
xdotool mousemove --window $WID 760 531 click 1; sleep 0.6
xdotool mousemove --window $WID 760 132 click 1; sleep 0.6
xdotool mousemove --window $WID 845 26 click 1; sleep 0.6
xdotool mousemove --window $WID 760 561 click 1; sleep 0.6
xdotool mousemove --window $WID 760 132 click 1; sleep 0.8
snap ../shots/ch15-light.png
