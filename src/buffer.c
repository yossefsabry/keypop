#include <string.h>
#include "buffer.h"

static void buf_shift_left(struct client_state *state) {
    if (state->seg_count == 0) return;
    int len_to_remove = state->seg_lengths[0];
    memmove(state->display_buf, state->display_buf + len_to_remove, state->display_len - len_to_remove + 1);
    state->display_len -= len_to_remove;
    memmove(state->seg_lengths, state->seg_lengths + 1, (state->seg_count - 1) * sizeof(int));
    state->seg_count--;
}

void buf_append(struct client_state *state, const char *text) {
    size_t text_len = strlen(text);
    if (text_len == 0) return;

    if (state->seg_count >= MAX_SEGMENTS) buf_shift_left(state);

    while (state->display_len + text_len >= MAX_DISPLAY_LEN) {
        buf_shift_left(state);
        // Safety break if buffer is full of one huge segment (unlikely)
        if (state->seg_count == 0) {
            state->display_len = 0;
            break;
        }
    }

    memcpy(state->display_buf + state->display_len, text, text_len + 1);
    state->display_len += text_len;
    state->seg_lengths[state->seg_count++] = text_len;
}

void buf_backspace(struct client_state *state) {
    if (state->seg_count == 0) return;
    int len_to_remove = state->seg_lengths[--state->seg_count];
    if (len_to_remove > (int)state->display_len) len_to_remove = state->display_len;
    state->display_len -= len_to_remove;
    state->display_buf[state->display_len] = '\0';
}

void buf_delete_word(struct client_state *state) {
    if (state->seg_count == 0) return;

    // 1. Remove trailing spaces if any (treat sequence of spaces as one unit to delete, or just skip them to get to the word)
    // Standard Ctrl+W/Ctrl+Backspace usually deletes the word AND the trailing space if you are just after a word.
    // If we are at "Hello World ", it deletes "World ".
    // If we are at "Hello World  ", it might delete just spaces or the word too.
    // Let's adopt a simple approach: Delete backwards until we find a non-space, then delete until we find a space.
    
    // However, our buffer is segmented. We must respect segments to keep `seg_lengths` valid.
    
    int deleted_something = 0;
    
    // Step 1: Consume trailing spaces
    while (state->seg_count > 0) {
        int last_seg_idx = state->seg_count - 1;
        int last_len = state->seg_lengths[last_seg_idx];
        char *last_seg_start = state->display_buf + state->display_len - last_len;
        
        // Check if this segment is a space (our keys.c actually adds " " as a separate segment usually)
        if (strcmp(last_seg_start, " ") == 0) {
            buf_backspace(state);
            deleted_something = 1;
        } else {
            break; 
        }
    }
    
    // Step 2: Consume the word (non-space segments)
    while (state->seg_count > 0) {
        int last_seg_idx = state->seg_count - 1;
        int last_len = state->seg_lengths[last_seg_idx];
        char *last_seg_start = state->display_buf + state->display_len - last_len;
        
        // If we hit a space, stop
        if (strcmp(last_seg_start, " ") == 0) {
            if (!deleted_something) {
               // If we haven't deleted anything yet (e.g. cursor was right at the space), 
               // we should technically delete this space and continue? 
               // Actually, `Ctrl+W` behaving like vim/bash:
               // "Start " -> "Start" (deletes space)
               // "Start" -> "" (deletes word)
               // My logic above in Step 1 handles the "Start " -> "Start" case if there are trailing spaces.
               // So if we are here, we hit a space *after* deleting some non-space stuff, so we stop.
            }
            break;
        }
        
        // Delete this segment
        buf_backspace(state);
        deleted_something = 1;
    }
}
