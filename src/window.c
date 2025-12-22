#include "window.h"
#include "draw.h"

static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct client_state *state = data;
    xdg_surface_ack_configure(surface, serial);
    if (state->window_visible) redraw(state);
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_configure };

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t w, int32_t h, struct wl_array *states) {
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

void window_create(struct client_state *state) {
    state->surface = wl_compositor_create_surface(state->compositor);
    state->xdg_surface = xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->surface);
    xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);
    
    state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
    xdg_toplevel_add_listener(state->xdg_toplevel, &xdg_toplevel_listener, state);
    xdg_toplevel_set_app_id(state->xdg_toplevel, "keypop");
    xdg_toplevel_set_title(state->xdg_toplevel, "Show Me The Key");
    wl_surface_commit(state->surface);
}

void hide_window(struct client_state *state) {
    if (!state->window_visible) return;
    state->window_visible = 0;
    state->display_buf[0] = '\0';
    state->display_len = 0;
    state->seg_count = 0;
    wl_surface_attach(state->surface, NULL, 0, 0);
    wl_surface_commit(state->surface);
}
