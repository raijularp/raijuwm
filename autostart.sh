#!/bin/sh
# Autostart helper for raijuwm.
# Make this executable and use it from ~/.xinitrc or from a desktop session.

# Start custom programs here.
# Example: set a background, start systray, and run a bar.

xsetroot -solid "#111111"
# Uncomment the next line to start a custom panel instead of the built-in bar.
# your-bar-command &

# Enable NumLock if desired.
# numlockx on &

# Start raijuwm itself.
exec raijuwm
