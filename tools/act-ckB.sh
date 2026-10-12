F=$XDG_CONFIG_HOME/gtklab/sessions.ini
xdotool mousemove --window $WID 120 143 click --repeat 2 --delay 80 1; sleep 0.5
printf '\n[core3-den]\nfolder=Core\nhost=10.0.0.3\nuser=admin\n' >> $F
xdotool key ctrl+shift+r; sleep 0.6
snap ../shots/ckB-reloaded.png
cp $F /tmp/good.ini; printf '\n[broken]\nfolder=Core\nhost=10.0.0.9\nport=abc\n' >> $F
xdotool key ctrl+shift+r; sleep 0.6
snap ../shots/ckB-error.png
cp /tmp/good.ini $F; xdotool key ctrl+shift+r; sleep 0.6
