#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <libudev.h>
#include <libinput.h>
#include <errno.h>
#include "input.h"

struct input_state {
    struct libinput *li;
    struct udev *udev;
    key_handler_t handler;
    void *user_data;
};

static int open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

struct input_state *input_init(key_handler_t handler, void *user_data) {
    struct input_state *state = calloc(1, sizeof(*state));
    state->handler = handler;
    state->user_data = user_data;

    state->udev = udev_new();
    if (!state->udev) {
        fprintf(stderr, "Failed to initialize udev\n");
        free(state);
        return NULL;
    }

    state->li = libinput_udev_create_context(&interface, NULL, state->udev);
    if (!state->li) {
        fprintf(stderr, "Failed to initialize libinput\n");
        udev_unref(state->udev);
        free(state);
        return NULL;
    }

    libinput_udev_assign_seat(state->li, "seat0");
    return state;
}

void input_destroy(struct input_state *state) {
    libinput_unref(state->li);
    udev_unref(state->udev);
    free(state);
}

int input_get_fd(struct input_state *state) {
    return libinput_get_fd(state->li);
}

void input_dispatch(struct input_state *state) {
    libinput_dispatch(state->li);
    
    struct libinput_event *event;
    while ((event = libinput_get_event(state->li))) {
        enum libinput_event_type type = libinput_event_get_type(event);
        
        if (type == LIBINPUT_EVENT_KEYBOARD_KEY) {
            struct libinput_event_keyboard *k = libinput_event_get_keyboard_event(event);
            uint32_t key = libinput_event_keyboard_get_key(k);
            uint32_t key_state = libinput_event_keyboard_get_key_state(k);
            
            if (state->handler) {
                state->handler(state->user_data, key, key_state);
            }
        }
        
        libinput_event_destroy(event);
    }
}
