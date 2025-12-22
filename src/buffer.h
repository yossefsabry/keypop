#ifndef BUFFER_H
#define BUFFER_H

#include "state.h"

void buf_append(struct client_state *state, const char *text);
void buf_backspace(struct client_state *state);

#endif
