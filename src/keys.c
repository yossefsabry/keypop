#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libinput.h>
#include "keys.h"
#include "buffer.h"

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
        case XKB_KEY_Control_L: case XKB_KEY_Control_R: return "Ctrl";
        case XKB_KEY_Alt_L: case XKB_KEY_Alt_R:        return "Alt";
        case XKB_KEY_Super_L: case XKB_KEY_Super_R:      return "Super";
        case XKB_KEY_Shift_L: case XKB_KEY_Shift_R:      return "Shift";
        case XKB_KEY_Delete:       return "Del";
        case XKB_KEY_Home:         return "Home";
        case XKB_KEY_End:          return "End";
        case XKB_KEY_Prior:        return "PgUp";
        case XKB_KEY_Next:         return "PgDn";
        case XKB_KEY_Caps_Lock:    return "Caps";
        case XKB_KEY_Num_Lock:     return "Num";
        case XKB_KEY_F1: return "F1"; case XKB_KEY_F2: return "F2";
        case XKB_KEY_F3: return "F3"; case XKB_KEY_F4: return "F4";
        case XKB_KEY_F5: return "F5"; case XKB_KEY_F6: return "F6";
        case XKB_KEY_F7: return "F7"; case XKB_KEY_F8: return "F8";
        case XKB_KEY_F9: return "F9"; case XKB_KEY_F10: return "F10";
        case XKB_KEY_F11: return "F11"; case XKB_KEY_F12: return "F12";
        case XKB_KEY_XF86AudioLowerVolume: return "Vol-";
        case XKB_KEY_XF86AudioRaiseVolume: return "Vol+";
        case XKB_KEY_XF86AudioMute:        return "Mute";
        case XKB_KEY_XF86MonBrightnessUp:  return "Bri+";
        case XKB_KEY_XF86MonBrightnessDown:return "Bri-";
        case XKB_KEY_XF86AudioPlay:        return "Play";
        case XKB_KEY_XF86AudioPrev:        return "Prev";
        case XKB_KEY_XF86AudioNext:        return "Next";
        default: return NULL;
    }
}

static inline void show_window(struct client_state *state) {
    state->window_visible = 1;
}

static int is_modifier(xkb_keysym_t key) {
    return (key == XKB_KEY_Control_L || key == XKB_KEY_Control_R ||
            key == XKB_KEY_Alt_L || key == XKB_KEY_Alt_R ||
            key == XKB_KEY_Super_L || key == XKB_KEY_Super_R ||
            key == XKB_KEY_Shift_L || key == XKB_KEY_Shift_R);
}



static void process_key_action(struct client_state *state, uint32_t key) {
    uint32_t xkb_keycode = key + 8;
    xkb_state_update_key(state->xkb_state, xkb_keycode, XKB_KEY_DOWN);
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, xkb_keycode);
    
    int is_mod = is_modifier(keysym);
    if (keysym == XKB_KEY_Control_L || keysym == XKB_KEY_Control_R) state->ctrl_pressed = 1;
    if (keysym == XKB_KEY_Alt_L || keysym == XKB_KEY_Alt_R) state->alt_pressed = 1;
    if (keysym == XKB_KEY_Shift_L || keysym == XKB_KEY_Shift_R) state->shift_pressed = 1;
    if (keysym == XKB_KEY_Super_L || keysym == XKB_KEY_Super_R) state->super_pressed = 1;

    if (state->overlay_enabled) {
        show_window(state);
        clock_gettime(CLOCK_MONOTONIC, &state->last_key_time);

        if (keysym == XKB_KEY_BackSpace) {
            if (state->ctrl_pressed) {
                buf_delete_word(state);
            } else {
                buf_backspace(state);
            }
        } else if (state->ctrl_pressed && keysym == XKB_KEY_w) {
            buf_delete_word(state);
        } else if (!is_mod) {
            char combined_buf[64] = {0};
            if (state->ctrl_pressed) strcat(combined_buf, "Ctrl+");
            if (state->alt_pressed) strcat(combined_buf, "Alt+");
            if (state->super_pressed) strcat(combined_buf, "Super+");
            
            const char *sym = get_key_symbol(keysym);
            char key_str[32] = {0};
            if (sym) strcpy(key_str, sym);
            else if (keysym == XKB_KEY_space) strcpy(key_str, " ");
            else {
                if (keysym >= 0x20 && keysym <= 0x7E) snprintf(key_str, sizeof(key_str), "%c", (char)keysym);
                else {
                    xkb_state_key_get_utf8(state->xkb_state, xkb_keycode, key_str, sizeof(key_str));
                    if (strlen(key_str) == 0 || (unsigned char)key_str[0] < 32)
                        if (keysym < 256 && keysym >= 32) snprintf(key_str, sizeof(key_str), "%c", (char)keysym);
                }
            }
            strcat(combined_buf, key_str);
            if (strlen(combined_buf) > 0) {
                if (state->seg_count > 0) {
                    int last_len = state->seg_lengths[state->seg_count - 1];
                    int this_len = strlen(combined_buf);
                    int prev_is_special = (last_len > 1 && state->display_buf[state->display_len - 1] != ' ');
                    int this_is_special = (this_len > 1 && combined_buf[0] != ' ');
                    if (prev_is_special || this_is_special) buf_append(state, " ");
                }
                buf_append(state, combined_buf);
            }
        }
        state->needs_redraw = 1;
    }
}

// Timer for subsequent repeats (rate)
static gboolean repeat_rate_tick(gpointer data) {
    struct client_state *state = data;
    process_key_action(state, state->repeat_key);
    return TRUE; // Continue repeating
}

// Timer for initial delay
static gboolean repeat_delay_done(gpointer data) {
    struct client_state *state = data;
    // Execute once
    process_key_action(state, state->repeat_key);
    
    // Switch to rate timer
    if (state->repeat_rate > 0) {
        state->repeat_timer_id = g_timeout_add(1000 / state->repeat_rate, repeat_rate_tick, state);
    } else {
        state->repeat_timer_id = 0;
    }
    return FALSE; // Stop the delay timer
}

void handle_key(void *data, uint32_t key, uint32_t state_val) {
    struct client_state *state = data;
    uint32_t xkb_keycode = key + 8;

    // Check if it's a modifier key
    // We need to peek at the keysym before updating state in process_key_action
    // Note: This relies on the current state mapping.
    xkb_keysym_t raw_sym = xkb_state_key_get_one_sym(state->xkb_state, xkb_keycode);
    int is_mod_key = is_modifier(raw_sym);

    if (state_val == LIBINPUT_KEY_STATE_PRESSED) {
        // Guard: If it's a modifier and we think it's already pressed, ignore this repeat.
        // This handles case where libinput sends repeats OR we messed up logic.
        if (is_mod_key) {
             if ((raw_sym == XKB_KEY_Shift_L || raw_sym == XKB_KEY_Shift_R) && state->shift_pressed) return;
             if ((raw_sym == XKB_KEY_Control_L || raw_sym == XKB_KEY_Control_R) && state->ctrl_pressed) return;
             if ((raw_sym == XKB_KEY_Alt_L || raw_sym == XKB_KEY_Alt_R) && state->alt_pressed) return;
             if ((raw_sym == XKB_KEY_Super_L || raw_sym == XKB_KEY_Super_R) && state->super_pressed) return;
        }

        // Cancel existing timer if any
        if (state->repeat_timer_id) {
            g_source_remove(state->repeat_timer_id);
            state->repeat_timer_id = 0;
        }

        // Process the key immediately
        process_key_action(state, key);
        
        // Setup repeat if enabled (but NOT for modifiers)
        // process_key_action already updated xkb_state, so we can just query the keysym
        xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, xkb_keycode);
        if (!is_modifier(keysym) && state->repeat_rate > 0 && state->repeat_delay > 0) {
            state->repeat_key = key;
            state->repeat_timer_id = g_timeout_add(state->repeat_delay, repeat_delay_done, state);
        }
        
    } else {
        // Key Release
        if (state->repeat_key == key) {
            if (state->repeat_timer_id) {
                g_source_remove(state->repeat_timer_id);
                state->repeat_timer_id = 0;
            }
            state->repeat_key = 0;
        }

        xkb_state_update_key(state->xkb_state, xkb_keycode, XKB_KEY_UP);
        xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, xkb_keycode);
        if (keysym == XKB_KEY_Control_L || keysym == XKB_KEY_Control_R) state->ctrl_pressed = 0;
        if (keysym == XKB_KEY_Alt_L || keysym == XKB_KEY_Alt_R) state->alt_pressed = 0;
        if (keysym == XKB_KEY_Shift_L || keysym == XKB_KEY_Shift_R) state->shift_pressed = 0;
        if (keysym == XKB_KEY_Super_L || keysym == XKB_KEY_Super_R) state->super_pressed = 0;
    }
}
