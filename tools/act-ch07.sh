xdotool mousemove --window $WID 745 26 click 1; sleep 0.6
snap ../shots/ch07-menu.png
xdotool key Escape; sleep 0.3
# three quick toggles: one debounced save
xdotool key ctrl+shift+b; sleep 0.1; xdotool key ctrl+shift+b; sleep 0.1; xdotool key ctrl+shift+b; sleep 1
snap ../shots/ch07-nosidebar.png
xdotool key ctrl+shift+b; sleep 1
# dark via the menu: open, hover Appearance, pick Dark
xdotool mousemove --window $WID 745 26 click 1; sleep 0.6
snap ../shots/ch07-menu2.png
xdotool mousemove --window $WID 700 150 click 1; sleep 0.6
snap ../shots/ch07-appearance.png
xdotool mousemove --window $WID 690 132 click 1; sleep 0.8
snap ../shots/ch07-dark.png
