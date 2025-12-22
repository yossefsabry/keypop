#ifndef STATE_H
#define STATE_H

#include <time.h>
#include <glib.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#include "input.h"

#define WINDOW_WIDTH 840
#define WINDOW_HEIGHT 130
#define PADDING 10
#define TOP_BOTTOM_PADDING 5
#define RIGHT_PADDING 60
#define MAX_DISPLAY_LEN 256
#define MAX_SEGMENTS 128
#define HIDE_TIMEOUT_MS 2000

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct client_state {
    // Wayland objects
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;
    
    // Input
    struct input_state *input;
    struct xkb_context *xkb_ctx;
    struct xkb_keymap *xkb_map;
    struct xkb_state *xkb_state;
    
    // Display state
    char display_buf[MAX_DISPLAY_LEN];
    size_t display_len;
    
    // Segment tracking for atomic backspace
    int seg_lengths[MAX_SEGMENTS];
    int seg_count;
    
    struct timespec last_key_time;
    
    // Flags
    unsigned int running : 1;
    unsigned int window_visible : 1;
    unsigned int needs_redraw : 1;
    unsigned int overlay_enabled : 1;  // Controls whether app shows when typing
    
    // Modifiers state
    unsigned int ctrl_pressed : 1;
    unsigned int alt_pressed : 1;
    unsigned int shift_pressed : 1;
    unsigned int super_pressed : 1;

    // GLib Main Loop
    GMainLoop *loop;
};

#endif
