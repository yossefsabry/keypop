# Show Me The Key - Hyprland Configuration

Add these lines to your `~/.config/hypr/hyprland.conf`:

```conf
# Show Me The Key window rules
windowrulev2 = float, class:^(keypop)$
windowrulev2 = pin, class:^(keypop)$
windowrulev2 = noblur, class:^(keypop)$
windowrulev2 = noshadow, class:^(keypop)$
windowrulev2 = noborder, class:^(keypop)$
windowrulev2 = nofocus, class:^(keypop)$
windowrulev2 = opaque, class:^(keypop)$
windowrulev2 = nodim, class:^(keypop)$
windowrulev2 = noanim, class:^(keypop)$
windowrulev2 = rounding 0, class:^(keypop)$
windowrulev2 = move 100%-820 100%-120, class:^(keypop)$
```

This will:
- Float the window (not tiled)
- Pin it (visible on all workspaces, always on top)
- Disable blur effect
- Disable shadow
- Remove border
- Prevent focus stealing
- Position at bottom-right corner
