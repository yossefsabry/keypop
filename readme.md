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

Requires access to input devices:
```bash
sudo -E ./keypop
```

Or add your user to the `input` group:
```bash
sudo usermod -aG input $USER
# Log out and back in, then:
./keypop
```

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
