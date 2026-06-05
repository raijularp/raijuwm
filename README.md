# raijuwm — Minimal tiling X11 window manager (C, Xlib)

raijuwm is a tiny, native Xlib-based tiling window manager written in C. It is designed to be minimal and dependency-free (only Xlib). It focuses on tiling-only workflows, configurable gaps, and simple, patchable configuration.

Build

Make sure you have X11 development libraries installed, then:

```sh
cd raijuwm
make
```

Run

Running a window manager requires you are not already running one on the same display. For safe testing, use a nested X server like Xephyr.

The repository includes a helper script:

```sh
cd raijuwm
make
./run.sh
(This isnt the recommend way to start raijuwm, just put exec raijuwm in your .xinitrc file :D) 
```

If you want to run manually:

```sh
# start nested X server
Xephyr :1 -screen 1920x1080 &
# run raijuwm inside it
DISPLAY=:1 ./raijuwm &
DISPLAY=:1 ./bar &
```

If you disable the built-in bar in `config.h` with `use_bar = 0`, start your own bar instead:

```sh
DISPLAY=:1 ./raijuwm &
DISPLAY=:1 ./your-bar &
```

There is also an `autostart.sh` helper script in the repo that you can use from `~/.xinitrc`:

```sh
chmod +x autostart.sh
~/.config/raijuwm/autostart.sh
```

Controls

- Alt+Return — spawn default terminal (configurable)
- Alt+q — kill focused window
- Alt+p — start `dmenu_run`
- Alt+Space — cycle layouts (master-stack, monocle)
- Alt+Up / Alt+Down — focus previous/next window in the current workspace
- Alt+Left / Alt+Right — switch to the previous/next workspace
- Multi-monitor support via Xinerama (windows are distributed per screen)

Styling

raijuwm supports inner and outer gaps as well as configurable border width and colors via `config.h` for a tidy, modern look.

Native 1920x1080 mode

By default, `config.def.h` is configured to force a single 1920x1080 display. This makes it easy to run in a VM or Xephyr session without depending on the host display geometry.

Interaction

- Mod + Left-click and drag: move the focused window
- Mod + Right-click and drag: resize the focused window
- Send `SIGHUP` to the process to trigger a restart (useful after rebuilding with changes to `config.h`).

Installation

To install system-wide (copies binaries to `/usr/local/bin`):

```sh
cd raijuwm
sudo make install
```

To remove installed binaries:

```sh
cd raijuwm
sudo make uninstall
```


Configuration

This project uses a suckless-style patchable `config.h`. Edit `config.h` to change keybindings, the default terminal, or the forced native display mode.

The default config includes:
- `MOD` — modifier mask (default: Alt / `Mod1Mask`)
- `termcmd` — default terminal command
- `dmenucmd` — command used to run dmenu
- `use_bar` — set to `0` to disable the built-in bar if you want to use your own panel
- `mfactor` — master area factor for the master-stack layout
- `force_native_display` — set to `1` to use a fixed 1920x1080 screen, or `0` to honor actual display geometry
- `native_screen_width` / `native_screen_height` — the forced display resolution

If `config.h` is missing, `make` will copy `config.def.h` to `config.h` so you can modify it.

Notes

- This is intentionally minimal and not feature-complete. Use it as a starting point for adding keybindings, layouts, configuration, and window decorations.
