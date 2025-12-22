#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <glib.h>
#include <wayland-client.h>
#include "state.h"
#include "wl_setup.h"
#include "window.h"
#include "keys.h"
#include "draw.h"
#include "tray.h"

// Helper for time
static inline long time_diff_ms(const struct timespec *start, const struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1000000;
}

// GLib callbacks
static gboolean on_wayland_event(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)source;
    struct client_state *state = data;
    if (condition & G_IO_IN) {
        if (wl_display_dispatch(state->display) == -1) return FALSE;
    }
    if (condition & (G_IO_ERR | G_IO_HUP)) return FALSE;
    return TRUE;
}

static gboolean on_input_event(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)source;
    struct client_state *state = data;
    if (condition & G_IO_IN) {
        input_dispatch(state->input);
    }
    return TRUE;
}

static gboolean on_timer_tick(gpointer data) {
    struct client_state *state = data;
    
    // Auto-hide logic
    if (state->window_visible) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (time_diff_ms(&state->last_key_time, &now) > HIDE_TIMEOUT_MS) {
            hide_window(state);
            state->needs_redraw = 0; // hide_window commits, so no redraw needed
        }
    }

    // Redraw logic
    if (state->needs_redraw && state->window_visible) {
        redraw(state);
        state->needs_redraw = 0;
    }
    
    // Flush wayland
    if (wl_display_flush(state->display) < 0 && errno != EAGAIN) {
        return FALSE; // Error
    }

    return TRUE; // Continue calling
}

// Helper to parse hex color
// Helper to parse hex color
static void parse_color(const char *hex, double *rgba) {
    if (!hex) return;
    if (hex[0] == '#') hex++; // skip # if present
    
    int r = 0, g = 0, b = 0, a = 255;
    int len = strlen(hex);
    
    if (len == 6) {
        sscanf(hex, "%02x%02x%02x", &r, &g, &b);
    } else if (len == 8) {
        sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a);
    }
    
    rgba[0] = r / 255.0;
    rgba[1] = g / 255.0;
    rgba[2] = b / 255.0;
    rgba[3] = a / 255.0;
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -b <color>   Set background color (e.g. #000000 or 000000)\n");
    printf("  -c <color>   Set text color (e.g. #FFFFFF or FFFFFF)\n");
    printf("  -s <size>    Set font size (default: 65)\n");
    printf("  -g <WxH>     Set window size (default: 840x130)\n");
    printf("  -o <opacity> Set background opacity (0.0 - 1.0)\n");
    printf("  -h           Show this help\n");
}

int main(int argc, char *argv[]) {
    struct client_state state = {0};
    state.running = 1;
    state.overlay_enabled = 1; // Default to shown
    clock_gettime(CLOCK_MONOTONIC, &state.last_key_time);
    
    // Default config
    state.width = DEFAULT_WIDTH;
    state.height = DEFAULT_HEIGHT;

    state.bg_color[0] = 0.0;
    state.bg_color[1] = 0.0;
    state.bg_color[2] = 0.0;
    state.bg_color[3] = 0.6; // Default 60% alpha black

    state.text_color[0] = 1.0;
    state.text_color[1] = 1.0;
    state.text_color[2] = 1.0;
    state.text_color[3] = 1.0; // Default White

    state.font_size = 65;

    int opt;
    while ((opt = getopt(argc, argv, "b:c:s:g:o:h")) != -1) {
        switch (opt) {
            case 'b':
                parse_color(optarg, state.bg_color);
                break;
            case 'c':
                parse_color(optarg, state.text_color);
                break;
            case 's':
                state.font_size = atoi(optarg);
                if (state.font_size < 10) state.font_size = 10;
                break;
            case 'g':
                sscanf(optarg, "%dx%d", &state.width, &state.height);
                if (state.width < 100) state.width = 100;
                if (state.height < 50) state.height = 50;
                break;
            case 'o': {
                float opacity = atof(optarg);
                if (opacity < 0.0f) opacity = 0.0f;
                if (opacity > 1.0f) opacity = 1.0f;
                state.bg_color[3] = opacity;
                break;
            }
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Initialize subsystems
    if (wl_setup_connect(&state) != 0) {
        fprintf(stderr, "Failed to connect to Wayland\n");
        return 1;
    }

    state.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    state.xkb_map = xkb_keymap_new_from_names(state.xkb_ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    state.xkb_state = xkb_state_new(state.xkb_map);
    if (!state.xkb_ctx || !state.xkb_map || !state.xkb_state) return 1;

    state.input = input_init(handle_key, &state);
    if (!state.input) fprintf(stderr, "Warning: Failed to init input\n");

    window_create(&state);
    
    // Setup Tray
    tray_init(&state);

    // Setup GMainLoop
    state.loop = g_main_loop_new(NULL, FALSE);

    // Add Wayland fd
    GIOChannel *wl_chan = g_io_channel_unix_new(wl_display_get_fd(state.display));
    g_io_add_watch(wl_chan, G_IO_IN | G_IO_ERR | G_IO_HUP, on_wayland_event, &state);
    g_io_channel_unref(wl_chan);

    // Add Input fd
    if (state.input) {
        GIOChannel *in_chan = g_io_channel_unix_new(input_get_fd(state.input));
        g_io_add_watch(in_chan, G_IO_IN, on_input_event, &state);
        g_io_channel_unref(in_chan);
    }

    // Add Timer (approx 60fps or less, just for auto-hide checks and flush)
    // 16ms = ~60fps
    g_timeout_add(16, on_timer_tick, &state);

    // Initial Flush
    wl_display_roundtrip(state.display);

    // Run Loop
    g_main_loop_run(state.loop);

    // Cleanup
    if (state.buffer) wl_buffer_destroy(state.buffer);
    if (state.input) input_destroy(state.input);
    // tray_destroy(&state); // Not strictly needed on exit
    xkb_state_unref(state.xkb_state);
    xkb_keymap_unref(state.xkb_map);
    xkb_context_unref(state.xkb_ctx);
    wl_setup_disconnect(&state);
    g_main_loop_unref(state.loop);
    
    return 0;
}
