#define _POSIX_C_SOURCE 200809L
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <cairo.h>
#include <poll.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#include "input.h"

// Configuration
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

// Application state
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
    
    // Modifiers state
    unsigned int ctrl_pressed : 1;
    unsigned int alt_pressed : 1;
    unsigned int shift_pressed : 1;
    unsigned int super_pressed : 1;
};

// --- Shared Memory Helpers ---

static int create_shm_file(void) {
    static unsigned int counter = 0;
    char name[32];
    snprintf(name, sizeof(name), "/wl_shm-%u-%u", getpid(), counter++);
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) shm_unlink(name);
    return fd;
}

static int allocate_shm_file(size_t size) {
    int fd = create_shm_file();
    if (fd < 0) return -1;
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static inline long time_diff_ms(const struct timespec *start, const struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1000000;
}

// --- Drawing ---

static void redraw(struct client_state *state) {
    if (!state->surface || !state->window_visible) return;

    const int stride = WINDOW_WIDTH * 4;
    const size_t size = stride * WINDOW_HEIGHT;
    
    int fd = allocate_shm_file(size);
    if (fd == -1) return;
    
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return;
    }
    
    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, 
        WINDOW_WIDTH, WINDOW_HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    
    // Create Cairo context
    cairo_surface_t *cs = cairo_image_surface_create_for_data(
        data, CAIRO_FORMAT_ARGB32, WINDOW_WIDTH, WINDOW_HEIGHT, stride);
    cairo_t *cr = cairo_create(cs);
    
    // Clear background
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // Draw rounded rectangle background
    const double r = 0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, WINDOW_WIDTH - r, r, r, -M_PI/2, 0);
    cairo_arc(cr, WINDOW_WIDTH - r, WINDOW_HEIGHT - r, r, 0, M_PI/2);
    cairo_arc(cr, r, WINDOW_HEIGHT - r, r, M_PI/2, M_PI);
    cairo_arc(cr, r, r, r, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.60);
    cairo_fill(cr);
    
    // Draw text
    if (state->display_len > 0) {
        cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 65);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        
        cairo_text_extents_t extents;
        cairo_font_extents_t font_extents;
        const int max_width = WINDOW_WIDTH - PADDING - RIGHT_PADDING;
        
        // Get font metrics for stable vertical alignment
        cairo_font_extents(cr, &font_extents);
        cairo_text_extents(cr, state->display_buf, &extents);
        
        const char *display_text = state->display_buf;
        char overflow_buf[MAX_DISPLAY_LEN + 4];
        
        if (extents.width > max_width) {
            for (size_t i = 1; i < state->display_len; i++) {
                if ((state->display_buf[i] & 0xC0) != 0x80) {
                    snprintf(overflow_buf, sizeof(overflow_buf), "...%s", 
                             state->display_buf + i);
                    cairo_text_extents(cr, overflow_buf, &extents);
                    if (extents.width <= max_width) {
                        display_text = overflow_buf;
                        break;
                    }
                }
            }
        }
        
        cairo_text_extents(cr, display_text, &extents);
        double x = WINDOW_WIDTH - RIGHT_PADDING - extents.width;
        if (x < PADDING) x = PADDING;
        
        // Use Font Metrics for Vertical Alignment
        // This ensures the baseline is consistent regardless of which characters are shown.
        // y = (WINDOW_HEIGHT - font_height) / 2 + ascent + offset
        // - 7 : it's a preference alignt u can change to fit u'r View
        double y = (WINDOW_HEIGHT - font_extents.height) / 2.0 + font_extents.ascent 
            + TOP_BOTTOM_PADDING - 7.0;
        
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, display_text);
    }
    
    cairo_destroy(cr);
    cairo_surface_destroy(cs);
    munmap(data, size);
    
    wl_surface_attach(state->surface, buffer, 0, 0);
    wl_surface_damage_buffer(state->surface, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    wl_surface_commit(state->surface);
    
    if (state->buffer) wl_buffer_destroy(state->buffer);
    state->buffer = buffer;
}

static void hide_window(struct client_state *state) {
    if (!state->window_visible) return;
    state->window_visible = 0;
    state->display_buf[0] = '\0';
    state->display_len = 0;
    state->seg_count = 0;
    wl_surface_attach(state->surface, NULL, 0, 0);
    wl_surface_commit(state->surface);
}

static inline void show_window(struct client_state *state) {
    state->window_visible = 1;
}

// --- Wayland Listeners ---

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(base, serial);
}
static const struct xdg_wm_base_listener xdg_wm_base_listener = { .ping = xdg_wm_base_ping };
static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct client_state *state = data;
    xdg_surface_ack_configure(surface, serial);
    if (state->window_visible) redraw(state);
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_configure };
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                                   int32_t w, int32_t h, struct wl_array *states) {
    (void)data; (void)toplevel; (void)w; (void)h; (void)states;
}
static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    (void)toplevel;
    ((struct client_state *)data)->running = 0;
}
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};
static void registry_global(void *data, struct wl_registry *reg, uint32_t name, 
                            const char *iface, uint32_t version) {
    (void)version;
    struct client_state *s = data;
    if (strcmp(iface, wl_compositor_interface.name) == 0)
        s->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    else if (strcmp(iface, wl_shm_interface.name) == 0)
        s->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    else if (strcmp(iface, xdg_wm_base_interface.name) == 0) {
        s->xdg_wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(s->xdg_wm_base, &xdg_wm_base_listener, s);
    }
}
static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name) { (void)data; (void)reg; (void)name; }
static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

// --- Key Symbol Lookup ---

static const char* get_key_symbol(xkb_keysym_t keysym) {
    switch (keysym) {
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:     return "Enter";
        case XKB_KEY_Tab:
        case XKB_KEY_ISO_Left_Tab: return "Tab";
        case XKB_KEY_Escape:       return "Esc";
        case XKB_KEY_Up:           return "Up";
        case XKB_KEY_Down:         return "Down";
        case XKB_KEY_Left:         return "Left";
        case XKB_KEY_Right:        return "Right";
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:    return "Ctrl";
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:        return "Alt";
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:      return "Super";
        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:      return "Shift";
        case XKB_KEY_Delete:       return "Del";
        case XKB_KEY_Home:         return "Home";
        case XKB_KEY_End:          return "End";
        case XKB_KEY_Prior:        return "PgUp";
        case XKB_KEY_Next:         return "PgDn";
        case XKB_KEY_Caps_Lock:    return "Caps";
        case XKB_KEY_Num_Lock:     return "Num";
        case XKB_KEY_F1:           return "F1";
        case XKB_KEY_F2:           return "F2";
        case XKB_KEY_F3:           return "F3";
        case XKB_KEY_F4:           return "F4";
        case XKB_KEY_F5:           return "F5";
        case XKB_KEY_F6:           return "F6";
        case XKB_KEY_F7:           return "F7";
        case XKB_KEY_F8:           return "F8";
        case XKB_KEY_F9:           return "F9";
        case XKB_KEY_F10:          return "F10";
        case XKB_KEY_F11:          return "F11";
        case XKB_KEY_F12:          return "F12";
        case XKB_KEY_XF86AudioLowerVolume: return "Vol-";
        case XKB_KEY_XF86AudioRaiseVolume: return "Vol+";
        case XKB_KEY_XF86AudioMute:        return "Mute";
        case XKB_KEY_XF86MonBrightnessUp:  return "Bri+";
        case XKB_KEY_XF86MonBrightnessDown:return "Bri-";
        case XKB_KEY_XF86AudioPlay:        return "Play";
        case XKB_KEY_XF86AudioPrev:        return "Prev";
        case XKB_KEY_XF86AudioNext:        return "Next";
        default:                   return NULL;
    }
}

// --- Text Buffer Management ---

static void buf_shift_left(struct client_state *state) {
    if (state->seg_count == 0) return;
    int len_to_remove = state->seg_lengths[0];
    memmove(state->display_buf, state->display_buf + len_to_remove, state->display_len - len_to_remove + 1);
    state->display_len -= len_to_remove;
    memmove(state->seg_lengths, state->seg_lengths + 1, (state->seg_count - 1) * sizeof(int));
    state->seg_count--;
}

static void buf_append(struct client_state *state, const char *text) {
    size_t text_len = strlen(text);
    if (text_len == 0) return;

    if (state->seg_count >= MAX_SEGMENTS) buf_shift_left(state);

    while (state->display_len + text_len >= MAX_DISPLAY_LEN) {
        buf_shift_left(state);
        if (state->seg_count == 0) {
            state->display_len = 0;
            break;
        }
    }

    memcpy(state->display_buf + state->display_len, text, text_len + 1);
    state->display_len += text_len;
    state->seg_lengths[state->seg_count++] = text_len;
}

static void buf_backspace(struct client_state *state) {
    if (state->seg_count == 0) return;
    int len_to_remove = state->seg_lengths[--state->seg_count];
    if (len_to_remove > (int)state->display_len) len_to_remove = state->display_len;
    state->display_len -= len_to_remove;
    state->display_buf[state->display_len] = '\0';
}

static int is_modifier(xkb_keysym_t key) {
    return (key == XKB_KEY_Control_L || key == XKB_KEY_Control_R ||
            key == XKB_KEY_Alt_L || key == XKB_KEY_Alt_R ||
            key == XKB_KEY_Super_L || key == XKB_KEY_Super_R ||
            key == XKB_KEY_Shift_L || key == XKB_KEY_Shift_R);
}

// --- Input Handling ---

static void handle_key(void *data, uint32_t key, uint32_t state_val) {
    struct client_state *state = data;
    uint32_t xkb_keycode = key + 8;

    if (state_val == LIBINPUT_KEY_STATE_PRESSED) {
        xkb_state_update_key(state->xkb_state, xkb_keycode, XKB_KEY_DOWN);
        xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, xkb_keycode);
        
        show_window(state);
        clock_gettime(CLOCK_MONOTONIC, &state->last_key_time);
        
        int is_mod = is_modifier(keysym);
        if (keysym == XKB_KEY_Control_L || keysym == XKB_KEY_Control_R) state->ctrl_pressed = 1;
        if (keysym == XKB_KEY_Alt_L || keysym == XKB_KEY_Alt_R) state->alt_pressed = 1;
        if (keysym == XKB_KEY_Shift_L || keysym == XKB_KEY_Shift_R) state->shift_pressed = 1;
        if (keysym == XKB_KEY_Super_L || keysym == XKB_KEY_Super_R) state->super_pressed = 1;

        if (keysym == XKB_KEY_BackSpace) {
            buf_backspace(state);
        } else if (is_mod) {
            // Modifiers tracked but not displayed alone
        } else {
            char combined_buf[64] = {0};
            
            if (state->ctrl_pressed) strcat(combined_buf, "Ctrl+");
            if (state->alt_pressed) strcat(combined_buf, "Alt+");
            if (state->super_pressed) strcat(combined_buf, "Super+");
            
            const char *sym = get_key_symbol(keysym);
            char key_str[32] = {0};
            
            if (sym) {
                strcpy(key_str, sym);
            } else if (keysym == XKB_KEY_space) {
                strcpy(key_str, " ");
            } else {
                if (keysym >= 0x20 && keysym <= 0x7E) {
                     snprintf(key_str, sizeof(key_str), "%c", (char)keysym);
                } else {
                    xkb_state_key_get_utf8(state->xkb_state, xkb_keycode, key_str, sizeof(key_str));
                    if (strlen(key_str) == 0 || (unsigned char)key_str[0] < 32) {
                        if (keysym < 256 && keysym >= 32)
                             snprintf(key_str, sizeof(key_str), "%c", (char)keysym);
                    }
                }
            }
            strcat(combined_buf, key_str);
            
            if (strlen(combined_buf) > 0) {
                // Determine spacing
                if (state->seg_count > 0) {
                    int last_len = state->seg_lengths[state->seg_count - 1];
                    int this_len = strlen(combined_buf);
                    int prev_is_special = (last_len > 1 && state->display_buf[state->display_len - 1] != ' ');
                    int this_is_special = (this_len > 1 && combined_buf[0] != ' ');
                    
                    if (prev_is_special || this_is_special) {
                         buf_append(state, " ");
                    }
                }
                buf_append(state, combined_buf);
            }
        }
        state->needs_redraw = 1;
    } else {
        xkb_state_update_key(state->xkb_state, xkb_keycode, XKB_KEY_UP);
        xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, xkb_keycode);
        
        if (keysym == XKB_KEY_Control_L || keysym == XKB_KEY_Control_R) state->ctrl_pressed = 0;
        if (keysym == XKB_KEY_Alt_L || keysym == XKB_KEY_Alt_R) state->alt_pressed = 0;
        if (keysym == XKB_KEY_Shift_L || keysym == XKB_KEY_Shift_R) state->shift_pressed = 0;
        if (keysym == XKB_KEY_Super_L || keysym == XKB_KEY_Super_R) state->super_pressed = 0;
    }
}

// --- Main ---

int main(void) {
    struct client_state state = {0};
    state.running = 1;
    clock_gettime(CLOCK_MONOTONIC, &state.last_key_time);

    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }
    
    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);
    
    if (!state.compositor || !state.shm || !state.xdg_wm_base) {
        fprintf(stderr, "Failed to bind required globals\n");
        return 1;
    }

    state.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    state.xkb_map = xkb_keymap_new_from_names(state.xkb_ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    state.xkb_state = xkb_state_new(state.xkb_map);
    
    if (!state.xkb_ctx || !state.xkb_map || !state.xkb_state) {
        fprintf(stderr, "Failed to init XKB\n");
        return 1;
    }

    state.input = input_init(handle_key, &state);
    if (!state.input) {
        fprintf(stderr, "Warning: Failed to init input (need input group?)\n");
    }

    state.surface = wl_compositor_create_surface(state.compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);
    xdg_toplevel_set_app_id(state.xdg_toplevel, "one.alynx.showmethekey");
    xdg_toplevel_set_title(state.xdg_toplevel, "Show Me The Key");
    wl_surface_commit(state.surface);

    struct pollfd fds[2];
    int wl_fd = wl_display_get_fd(state.display);
    int input_fd = state.input ? input_get_fd(state.input) : -1;

    while (state.running) {
        if (state.window_visible) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (time_diff_ms(&state.last_key_time, &now) > HIDE_TIMEOUT_MS) {
                hide_window(&state);
            }
        }
        
        if (state.needs_redraw && state.window_visible) {
            redraw(&state);
            state.needs_redraw = 0;
        }
        
        while (wl_display_prepare_read(state.display) != 0)
            wl_display_dispatch_pending(state.display);
        
        wl_display_flush(state.display);
        
        fds[0] = (struct pollfd){.fd = wl_fd, .events = POLLIN};
        fds[1] = (struct pollfd){.fd = input_fd, .events = POLLIN};
        int nfds = (input_fd >= 0) ? 2 : 1;
        
        if (poll(fds, nfds, 100) > 0) {
            if (fds[0].revents & POLLIN) {
                wl_display_read_events(state.display);
                wl_display_dispatch_pending(state.display);
            } else {
                wl_display_cancel_read(state.display);
            }
            if (nfds > 1 && (fds[1].revents & POLLIN)) {
                input_dispatch(state.input);
            }
        } else {
            wl_display_cancel_read(state.display);
        }
    }

    if (state.buffer) wl_buffer_destroy(state.buffer);
    if (state.input) input_destroy(state.input);
    xkb_state_unref(state.xkb_state);
    xkb_keymap_unref(state.xkb_map);
    xkb_context_unref(state.xkb_ctx);
    wl_display_disconnect(state.display);
    
    return 0;
}
