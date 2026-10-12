sleep 0.5
echo "=== binds at startup: $(grep -c ' bind ' ../shots/ch05.log)"
for i in $(seq 1 30); do xdotool mousemove --window $WID 130 300 click 5; done
sleep 0.5
echo "=== binds after scrolling: $(grep -c ' bind ' ../shots/ch05.log)"
