#!/bin/sh
# Run raijuwm and bar inside a nested Xephyr server at 1920x1080.
# Requires Xephyr and that raijuwm/bar are built.

if [ ! -x ./raijuwm ] || [ ! -x ./bar ]; then
    echo "Build raijuwm and bar first: make"
    exit 1
fi

Xephyr :1 -screen 1920x1080 &
XEYPHER_PID=$!
sleep 1

DISPLAY=:1 ./raijuwm &
WM_PID=$!

DISPLAY=:1 ./bar &
BAR_PID=$!

echo "Xephyr PID: $XEYPHER_PID"
echo "raijuwm PID: $WM_PID"
echo "bar PID: $BAR_PID"
echo "Use 'kill $WM_PID $BAR_PID $XEYPHER_PID' to stop."