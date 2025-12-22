#ifndef WL_SETUP_H
#define WL_SETUP_H

#include "state.h"

int wl_setup_connect(struct client_state *state);
void wl_setup_disconnect(struct client_state *state);

#endif
