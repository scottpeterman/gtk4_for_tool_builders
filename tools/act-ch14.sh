dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
dbl 120 105; dbl 120 143; sleep 1
xdotool key ctrl+shift+l; sleep 0.3
xdotool type --delay 15 'ls --color=always /usr/share | head -20; printf "\e[1;31mred\e[0m \e[42m green bg \e[0m \e[38;5;208m256-orange\e[0m\n"'; xdotool key Return; sleep 1
snap ../shots/ch14-colors.png
xdotool type --delay 15 'htop'; xdotool key Return; sleep 2
snap ../shots/ch14-htop.png
xdotool key q; sleep 0.5
xdotool key ctrl+equal ctrl+equal; sleep 0.6
snap ../shots/ch14-zoomed.png
xdotool key ctrl+0; sleep 0.3
# 67 MB through the pty: the UI must keep responding
xdotool type --delay 15 'time cat /tmp/big.txt'; xdotool key Return; sleep 1
T0=$(date +%s.%N); xdotool windowsize $WID 1000 650; xdotool mousemove --window $WID 745 26 click 1; sleep 0.5
snap ../shots/ch14-during-cat.png
T1=$(date +%s.%N); echo "menu during cat after $(echo "$T1 - $T0" | bc)s" >> /tmp/marks14.log
xdotool key Escape
for i in $(seq 1 60); do sleep 1; grep -q real ../shots/ch14.log 2>/dev/null && break; done
sleep 2
snap ../shots/ch14-cat-done.png
xdotool key ctrl+shift+l; sleep 0.3
