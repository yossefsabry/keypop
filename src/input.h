#ifndef INPUT_H
#define INPUT_H

#include <libinput.h>

struct input_state;

typedef void (*key_handler_t)(void *data, uint32_t key, uint32_t state);

struct input_state *input_init(key_handler_t handler, void *user_data);
void input_destroy(struct input_state *state);
int input_get_fd(struct input_state *state);
void input_dispatch(struct input_state *state);

#endif
