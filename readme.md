# Show Me The Key (Wayland)

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
sudo -E ./showmethekey
```

Or add your user to the `input` group:
```bash
sudo usermod -aG input $USER
# Log out and back in, then:
./showmethekey
```

## Hyprland Configuration

Add to `~/.config/hypr/hyprland.conf`:

```conf
# Show Me The Key - always on top, no effects
windowrulev2 = float, class:^(one.alynx.showmethekey)$
windowrulev2 = pin, class:^(one.alynx.showmethekey)$
windowrulev2 = noblur, class:^(one.alynx.showmethekey)$
windowrulev2 = noshadow, class:^(one.alynx.showmethekey)$
windowrulev2 = noborder, class:^(one.alynx.showmethekey)$
windowrulev2 = nofocus, class:^(one.alynx.showmethekey)$
windowrulev2 = move 100%-820 100%-100, class:^(one.alynx.showmethekey)$
```

## Exit

- Press `Ctrl+C` in terminal
- Or kill the process: `pkill showmethekey`
- Or close via Hyprland: `hyprctl dispatch killactive` with window focused

## Dependencies

- wayland-client
- wayland-protocols
- cairo
- libinput
- libudev
- xkbcommon
