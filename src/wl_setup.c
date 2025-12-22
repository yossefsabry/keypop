#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include "wl_setup.h"

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(base, serial);
}
static const struct xdg_wm_base_listener xdg_wm_base_listener = { .ping = xdg_wm_base_ping };

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

int wl_setup_connect(struct client_state *state) {
    state->display = wl_display_connect(NULL);
    if (!state->display) return -1;
    
    state->registry = wl_display_get_registry(state->display);
    wl_registry_add_listener(state->registry, &registry_listener, state);
    wl_display_roundtrip(state->display);
    
    if (!state->compositor || !state->shm || !state->xdg_wm_base) return -1;
    return 0;
}

void wl_setup_disconnect(struct client_state *state) {
    if (state->display) wl_display_disconnect(state->display);
}
