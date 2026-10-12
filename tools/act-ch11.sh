dbl() { xdotool mousemove --window $WID $1 $2 click --repeat 2 --delay 80 1; sleep 0.5; }
dbl 120 105; dbl 120 143      # Local -> localhost
sleep 0.5
xdotool type --delay 30 'echo hello from $0; uname -sr'; xdotool key Return; sleep 0.6
xdotool type --delay 20 'seq 200000 | tail -3'; xdotool key Return; sleep 0.8
snap ../shots/ch11-shell.png
xdotool type --delay 20 'seq 3000000'; xdotool key Return; sleep 4
# while that floods, a resize must still repaint
xdotool windowsize $WID 1000 650; sleep 1
# sessions file edited behind our back: 3 writes in quick succession -> one reload
F=$XDG_CONFIG_HOME/gtklab/sessions.ini
printf '\n[new-box]\nfolder=Lab\nhost=10.9.9.9\n' >> $F; sleep 0.05; touch $F; sleep 0.05; touch $F; sleep 1
xdotool type --delay 30 'exit'; xdotool key Return; sleep 1.2
snap ../shots/ch11-exited.png
