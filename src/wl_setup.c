#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "wl_setup.h"

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
    (void)data; (void)wl_keyboard; (void)format; (void)fd; (void)size;
    close(fd);
}
static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    (void)data; (void)wl_keyboard; (void)serial; (void)surface; (void)keys;
}
static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {
    (void)data; (void)wl_keyboard; (void)serial; (void)surface;
}
static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    (void)data; (void)wl_keyboard; (void)serial; (void)time; (void)key; (void)state;
}
static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    (void)data; (void)wl_keyboard; (void)serial; (void)mods_depressed; (void)mods_latched; (void)mods_locked; (void)group;
}
static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
    (void)wl_keyboard;
    struct client_state *state = data;
    state->repeat_rate = rate;
    state->repeat_delay = delay;
    // printf("Updated repeat settings: Rate=%d, Delay=%d\n", rate, delay);
}
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
    struct client_state *state = data;
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        state->wl_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(state->wl_keyboard, &keyboard_listener, state);
    }
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) { (void)data; (void)seat; (void)name; }
static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

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
    } else if (strcmp(iface, wl_seat_interface.name) == 0) {
        s->seat = wl_registry_bind(reg, name, &wl_seat_interface, 5);
        wl_seat_add_listener(s->seat, &seat_listener, s);
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
