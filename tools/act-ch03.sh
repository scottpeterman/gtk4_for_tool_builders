snap ../shots/ch03-empty.png
xdotool mousemove --window $WID 30 20 click 1; sleep 0.4
snap ../shots/ch03-tabs.png
# try to drag the divider fully left
xdotool mousemove --window $WID 262 300 mousedown 1 mousemove --window $WID 5 300 sleep 0.3 mouseup 1
snap ../shots/ch03-dragged.png
