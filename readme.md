# Keypop (Wayland)

A simple key display application for Wayland compositors like Hyprland.

## Features
- Shows typed keys on screen in real-time
- Window auto-hides after 2 seconds of inactivity
- Supports special keys (Enter, Tab, Ctrl, Alt, etc.)
- Optimized for low memory usage

## Build

```bash
make
```

## Install

```bash
sudo make install
```

## Run

Requires access to input devices. Add your user to the `input` group:

```bash
sudo usermod -aG input $USER
# Log out and back in
```

Then run:

```bash
./keypop
```

Or run with custom options:
```bash
# E.g., Blue background, Red text, Size 80, 1000x200 window, 80% opacity
./keypop -b "#0000FF" -c "#FF0000" -s 80 -g 1000x200 -o 0.8
```

Options:
- `-b <color>`: Background color hex (e.g. `#000000` or `000000`)
- `-c <color>`: Text color hex (e.g. `#FFFFFF` or `FFFFFF`)
- `-s <size>`: Font size (default 65)
- `-g <WxH>`: Window geometry (default 840x130)
- `-o <opacity>`: Background opacity (0.0 - 1.0)
- `-h`: Show help

## Hyprland Configuration

Add to `~/.config/hypr/hyprland.conf`:

```conf
# Keypop - always on top, no effects
windowrulev2 = float, class:^(keypop)$
windowrulev2 = pin, class:^(keypop)$
windowrulev2 = noblur, class:^(keypop)$
windowrulev2 = noshadow, class:^(keypop)$
windowrulev2 = noborder, class:^(keypop)$
windowrulev2 = nofocus, class:^(keypop)$
windowrulev2 = move 100%-820 100%-100, class:^(keypop)$
```

## Exit

- Press `Ctrl+C` in terminal
- Or kill the process: `pkill keypop`
- Or close via Hyprland: `hyprctl dispatch killactive` with window focused

## Dependencies

- wayland-client
- wayland-protocols
- cairo
- libinput
- libudev
- xkbcommon
