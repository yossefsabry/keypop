# Show Me The Key - Hyprland Configuration

Add these lines to your `~/.config/hypr/hyprland.conf`:

```conf
# Show Me The Key window rules
windowrulev2 = float, class:^(one.alynx.showmethekey)$
windowrulev2 = pin, class:^(one.alynx.showmethekey)$
windowrulev2 = noblur, class:^(one.alynx.showmethekey)$
windowrulev2 = noshadow, class:^(one.alynx.showmethekey)$
windowrulev2 = noborder, class:^(one.alynx.showmethekey)$
windowrulev2 = nofocus, class:^(one.alynx.showmethekey)$
windowrulev2 = move 100%-820 100%-100, class:^(one.alynx.showmethekey)$
```

This will:
- Float the window (not tiled)
- Pin it (visible on all workspaces, always on top)
- Disable blur effect
- Disable shadow
- Remove border
- Prevent focus stealing
- Position at bottom-right corner
