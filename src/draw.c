#define _POSIX_C_SOURCE 200809L
#define _USE_MATH_DEFINES
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>
#include <stdio.h>
#include <cairo.h>
#include "draw.h"
#include "shm.h"

// Helper to separate modifiers from key
// e.g., "Ctrl+Alt+Enter" -> mods="Ctrl+Alt+", key="Enter"
static void parse_segment(const char *segment, char *mods, char *key) {
    mods[0] = '\0';
    key[0] = '\0';
    
    // Find last '+'
    const char *last_plus = strrchr(segment, '+');
    if (last_plus) {
        // Check if the '+' is part of the key name itself?
        // Our keys.c generates "Ctrl+", "Alt+". The key itself relies on text.
        // If the key is "+", it would be "Ctrl++". 
        // strrchr finds the last one.
        // If key is "+", string is "Ctrl++". last_plus is the second +.
        // internal key is empty string? No.
        
        // Let's rely on the structure from keys.c
        // It builds "Mod+" then "Key".
        // If Key is "+", it is "Mod++" or just "+" (no mods).
        
        // Simple heuristic: If it ends with +, the key is +.
        // Wait, if I type +, keys.c says:
        // combined_buf = "Ctrl+" (if ctrl)
        // key_str = "+"
        // segment = "Ctrl++"
        
        // If I type "Enter", segment = "Enter" (no plus).
        // If I type "Ctrl+Enter", segment = "Ctrl+Enter".
        
        // If last char is +, then key is +.
        // "Ctrl++" -> last_plus is at end.
        
        size_t len = strlen(segment);
        if (len > 0 && segment[len-1] == '+') {
            // Case: "Ctrl++" (Key is +)
            // Or just "+" (Key is +)
            // We want to verify if there is a previous part.
            // Actually, if key is "+", we don't treat it as a SPECIAL icon key anyway.
            // So we can just fallback to text drawing.
            strcpy(key, segment);
            return;
        }

        // Normal case "Ctrl+Enter"
        // last_plus points to before 'Enter'
        int mod_len = last_plus - segment + 1;
        strncpy(mods, segment, mod_len);
        mods[mod_len] = '\0';
        strcpy(key, last_plus + 1);
    } else {
        strcpy(key, segment);
    }
}

// Draw specific icons
static void draw_icon(cairo_t *cr, const char *key_name, double x, double y, double size, const double *color) {
    cairo_save(cr);
    cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
    cairo_set_line_width(cr, size * 0.08);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (strcmp(key_name, "Enter") == 0) {
        // Return symbol ↵
        // Draw mostly in the em box at x, y (baseline)
        // Center vertically around y - size/3
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.4;
        double h = size * 0.3;
        
        cairo_move_to(cr, cx + w, cy - h);
        cairo_line_to(cr, cx, cy - h);
        cairo_line_to(cr, cx, cy);
        cairo_line_to(cr, cx - w/2, cy); // Arrow tip line
        
        // Arrow head
        cairo_move_to(cr, cx - w/2 + size*0.1, cy - size*0.1);
        cairo_line_to(cr, cx - w/2, cy);
        cairo_line_to(cr, cx - w/2 + size*0.1, cy + size*0.1);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Left") == 0) {
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.3;
        
        cairo_move_to(cr, cx + w, cy);
        cairo_line_to(cr, cx - w, cy);
        // Head
        cairo_move_to(cr, cx - w + size*0.15, cy - size*0.15);
        cairo_line_to(cr, cx - w, cy);
        cairo_line_to(cr, cx - w + size*0.15, cy + size*0.15);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Right") == 0) {
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.3;
        
        cairo_move_to(cr, cx - w, cy);
        cairo_line_to(cr, cx + w, cy);
        // Head
        cairo_move_to(cr, cx + w - size*0.15, cy - size*0.15);
        cairo_line_to(cr, cx + w, cy);
        cairo_line_to(cr, cx + w - size*0.15, cy + size*0.15);
        cairo_stroke(cr);

    } else if (strcmp(key_name, "Up") == 0) {
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double h = size * 0.3;
        
        cairo_move_to(cr, cx, cy + h);
        cairo_line_to(cr, cx, cy - h);
        // Head
        cairo_move_to(cr, cx - size*0.15, cy - h + size*0.15);
        cairo_line_to(cr, cx, cy - h);
        cairo_line_to(cr, cx + size*0.15, cy - h + size*0.15);
        cairo_stroke(cr);

    } else if (strcmp(key_name, "Down") == 0) {
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double h = size * 0.3;
        
        cairo_move_to(cr, cx, cy - h);
        cairo_line_to(cr, cx, cy + h);
        // Head
        cairo_move_to(cr, cx - size*0.15, cy + h - size*0.15);
        cairo_line_to(cr, cx, cy + h);
        cairo_line_to(cr, cx + size*0.15, cy + h - size*0.15);
        cairo_stroke(cr);

    } else if (strcmp(key_name, "Tab") == 0) {
        // Tab icon ->|
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.3;
        
        cairo_move_to(cr, cx - w, cy);
        cairo_line_to(cr, cx + w, cy);
        // Bar
        cairo_move_to(cr, cx + w, cy - size*0.15);
        cairo_line_to(cr, cx + w, cy + size*0.15);
        // Head
        cairo_move_to(cr, cx + w - size*0.15, cy - size*0.15);
        cairo_line_to(cr, cx + w, cy);
        cairo_line_to(cr, cx + w - size*0.15, cy + size*0.15);
        cairo_stroke(cr);
    
    } else if (strcmp(key_name, "Backspace") == 0 || strcmp(key_name, "BackSpace") == 0) {
        // Backspace: ⌫ (arrow with X)
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.25;
        
        // Arrow pointing left
        cairo_move_to(cr, cx + w, cy);
        cairo_line_to(cr, cx - w, cy);
        cairo_move_to(cr, cx - w + size*0.12, cy - size*0.12);
        cairo_line_to(cr, cx - w, cy);
        cairo_line_to(cr, cx - w + size*0.12, cy + size*0.12);
        
        // Small X on the right
        cairo_move_to(cr, cx + w - size*0.1, cy - size*0.08);
        cairo_line_to(cr, cx + w, cy + size*0.08);
        cairo_move_to(cr, cx + w, cy - size*0.08);
        cairo_line_to(cr, cx + w - size*0.1, cy + size*0.08);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Del") == 0 || strcmp(key_name, "Delete") == 0) {
        // Delete: forward arrow with X
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.25;
        
        // Arrow pointing right
        cairo_move_to(cr, cx - w, cy);
        cairo_line_to(cr, cx + w, cy);
        cairo_move_to(cr, cx + w - size*0.12, cy - size*0.12);
        cairo_line_to(cr, cx + w, cy);
        cairo_line_to(cr, cx + w - size*0.12, cy + size*0.12);
        
        // X on left
        cairo_move_to(cr, cx - w, cy - size*0.08);
        cairo_line_to(cr, cx - w + size*0.1, cy + size*0.08);
        cairo_move_to(cr, cx - w + size*0.1, cy - size*0.08);
        cairo_line_to(cr, cx - w, cy + size*0.08);
        cairo_stroke(cr);
        
    } else if (strncmp(key_name, "F", 1) == 0 && strlen(key_name) >= 2 && strlen(key_name) <= 3) {
        // Function keys F1-F12: Draw in rounded box with text
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double box_w = size * 0.45;
        double box_h = size * 0.35;
        
        // Rounded box
        cairo_set_line_width(cr, size * 0.05);
        cairo_rectangle(cr, cx - box_w/2, cy - box_h/2, box_w, box_h);
        cairo_stroke(cr);
        
        // Text (will be drawn as regular text, not icon path)
        // For now, just draw the outline
        cairo_restore(cr);
        return; // Let text rendering handle F-key label
        
    } else if (strcmp(key_name, "Caps") == 0) {
        // Caps Lock: A with up arrow
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double h = size * 0.25;
        
        // Up arrow above
        cairo_move_to(cr, cx, cy - h);
        cairo_line_to(cr, cx - size*0.1, cy - h + size*0.12);
        cairo_move_to(cr, cx, cy - h);
        cairo_line_to(cr, cx + size*0.1, cy - h + size*0.12);
        
        // "A" shape below
        cairo_move_to(cr, cx - size*0.15, cy + h);
        cairo_line_to(cr, cx, cy);
        cairo_line_to(cr, cx + size*0.15, cy + h);
        cairo_move_to(cr, cx - size*0.08, cy + h*0.4);
        cairo_line_to(cr, cx + size*0.08, cy + h*0.4);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Home") == 0) {
        // Home: House icon
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.25;
        
        // Roof (triangle)
        cairo_move_to(cr, cx - w, cy);
        cairo_line_to(cr, cx, cy - w);
        cairo_line_to(cr, cx + w, cy);
        // Base (rectangle)
        cairo_move_to(cr, cx - w*0.8, cy);
        cairo_line_to(cr, cx - w*0.8, cy + w);
        cairo_line_to(cr, cx + w*0.8, cy + w);
        cairo_line_to(cr, cx + w*0.8, cy);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "End") == 0) {
        // End: Corner arrow pointing down-right
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.25;
        
        // L-shape arrow
        cairo_move_to(cr, cx - w, cy - w);
        cairo_line_to(cr, cx - w, cy + w);
        cairo_line_to(cr, cx + w, cy + w);
        // Arrow head
        cairo_move_to(cr, cx + w - size*0.12, cy + w - size*0.12);
        cairo_line_to(cr, cx + w, cy + w);
        cairo_move_to(cr, cx + w, cy + w);
        cairo_line_to(cr, cx + w - size*0.12, cy + w + size*0.12);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "PgUp") == 0) {
        // Page Up: Double up arrows
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double h = size * 0.15;
        
        // First up arrow
        cairo_move_to(cr, cx, cy - h);
        cairo_line_to(cr, cx - size*0.12, cy - h + size*0.12);
        cairo_move_to(cr, cx, cy - h);
        cairo_line_to(cr, cx + size*0.12, cy - h + size*0.12);
        
        // Second up arrow below
        cairo_move_to(cr, cx, cy + h);
        cairo_line_to(cr, cx - size*0.12, cy + h + size*0.12);
        cairo_move_to(cr, cx, cy + h);
        cairo_line_to(cr, cx + size*0.12, cy + h + size*0.12);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "PgDn") == 0) {
        // Page Down: Double down arrows
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double h = size * 0.15;
        
        // First down arrow
        cairo_move_to(cr, cx, cy - h);
        cairo_line_to(cr, cx - size*0.12, cy - h - size*0.12);
        cairo_move_to(cr, cx, cy - h);
        cairo_line_to(cr, cx + size*0.12, cy - h - size*0.12);
        
        // Second down arrow below
        cairo_move_to(cr, cx, cy + h);
        cairo_line_to(cr, cx - size*0.12, cy + h - size*0.12);
        cairo_move_to(cr, cx, cy + h);
        cairo_line_to(cr, cx + size*0.12, cy + h - size*0.12);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Esc") == 0) {
        // Esc: Circle with X
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double r = size * 0.25;
        
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        cairo_stroke(cr);
        
        // X inside
        cairo_set_line_width(cr, size * 0.06);
        cairo_move_to(cr, cx - r*0.5, cy - r*0.5);
        cairo_line_to(cr, cx + r*0.5, cy + r*0.5);
        cairo_move_to(cr, cx + r*0.5, cy - r*0.5);
        cairo_line_to(cr, cx - r*0.5, cy + r*0.5);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Play") == 0) {
        // Play: Triangle
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double w = size * 0.2;
        
        cairo_move_to(cr, cx - w, cy - w);
        cairo_line_to(cr, cx + w, cy);
        cairo_line_to(cr, cx - w, cy + w);
        cairo_close_path(cr);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Pause") == 0) {
        // Pause: Two vertical bars
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        double h = size * 0.3;
        double w = size * 0.06;
        
        cairo_rectangle(cr, cx - size*0.12, cy - h/2, w, h);
        cairo_rectangle(cr, cx + size*0.06, cy - h/2, w, h);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Vol+") == 0 || strcmp(key_name, "Bri+") == 0) {
        // Volume/Brightness up: Speaker/sun with +
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        
        // + symbol
        cairo_move_to(cr, cx - size*0.15, cy);
        cairo_line_to(cr, cx + size*0.15, cy);
        cairo_move_to(cr, cx, cy - size*0.15);
        cairo_line_to(cr, cx, cy + size*0.15);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Vol-") == 0 || strcmp(key_name, "Bri-") == 0) {
        // Volume/Brightness down: - symbol
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        
        cairo_move_to(cr, cx - size*0.2, cy);
        cairo_line_to(cr, cx + size*0.2, cy);
        cairo_stroke(cr);
        
    } else if (strcmp(key_name, "Mute") == 0) {
        // Mute: Speaker with X
        double cx = x + size * 0.5;
        double cy = y - size * 0.3;
        
        // Speaker trapezoid
        cairo_move_to(cr, cx - size*0.2, cy - size*0.1);
        cairo_line_to(cr, cx - size*0.05, cy - size*0.2);
        cairo_line_to(cr, cx - size*0.05, cy + size*0.2);
        cairo_line_to(cr, cx - size*0.2, cy + size*0.1);
        cairo_close_path(cr);
        cairo_stroke(cr);
        
        // X through it
        cairo_move_to(cr, cx + size*0.05, cy - size*0.15);
        cairo_line_to(cr, cx + size*0.2, cy + size*0.15);
        cairo_move_to(cr, cx + size*0.2, cy - size*0.15);
        cairo_line_to(cr, cx + size*0.05, cy + size*0.15);
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

static int is_icon_key(const char *key) {
    // Check exact matches
    if (strcmp(key, "Enter") == 0 ||
        strcmp(key, "Left") == 0 ||
        strcmp(key, "Right") == 0 ||
        strcmp(key, "Up") == 0 ||
        strcmp(key, "Down") == 0 ||
        strcmp(key, "Tab") == 0 ||
        strcmp(key, "Backspace") == 0 ||
        strcmp(key, "BackSpace") == 0 ||
        strcmp(key, "Del") == 0 ||
        strcmp(key, "Delete") == 0 ||
        strcmp(key, "Caps") == 0 ||
        strcmp(key, "Home") == 0 ||
        strcmp(key, "End") == 0 ||
        strcmp(key, "PgUp") == 0 ||
        strcmp(key, "PgDn") == 0 ||
        strcmp(key, "Play") == 0 ||
        strcmp(key, "Pause") == 0 ||
        strcmp(key, "Prev") == 0 ||
        strcmp(key, "Next") == 0 ||
        strcmp(key, "Vol+") == 0 ||
        strcmp(key, "Vol-") == 0 ||
        strcmp(key, "Bri+") == 0 ||
        strcmp(key, "Bri-") == 0 ||
        strcmp(key, "Mute") == 0) {
        return 1;
    }
    
    // Check F-keys (F1-F12)
    if (key[0] == 'F' && strlen(key) >= 2 && strlen(key) <= 3) {
        // Could be F1-F12
        int fnum = atoi(key + 1);
        if (fnum >= 1 && fnum <= 12) {
            return 1;
        }
    }
    
    return 0;
}

void redraw(struct client_state *state) {
    if (!state->surface || !state->window_visible) return;

    const int stride = state->width * 4;
    const size_t size = stride * state->height;
    
    int fd = allocate_shm_file(size);
    if (fd == -1) return;
    
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) { close(fd); return; }
    
    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, 
        state->width, state->height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    
    cairo_surface_t *cs = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, state->width, state->height, stride);
    cairo_t *cr = cairo_create(cs);
    
    // Clear
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // Background
    const double r = 0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, state->width - r, r, r, -M_PI/2, 0);
    cairo_arc(cr, state->width - r, state->height - r, r, 0, M_PI/2);
    cairo_arc(cr, r, state->height - r, r, M_PI/2, M_PI);
    cairo_arc(cr, r, r, r, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, state->bg_color[0], state->bg_color[1], state->bg_color[2], state->bg_color[3]);
    cairo_fill(cr);
    
    // Font setup
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, state->font_size);
    cairo_set_source_rgba(cr, state->text_color[0], state->text_color[1], state->text_color[2], state->text_color[3]);

    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    
    const double icon_size = state->font_size;
    const double max_width = state->width - PADDING - RIGHT_PADDING;
    const double y_pos = (state->height - font_extents.height) / 2.0 + font_extents.ascent + TOP_BOTTOM_PADDING - 7.0;

    // Measurement & Logic Phase:
    // Determine which segments fit from the end.
    
    
    int start_seg = 0;
    double seg_widths[MAX_SEGMENTS];
    int seg_is_icon[MAX_SEGMENTS]; // 1 if key part is icon
    char seg_keys[MAX_SEGMENTS][32]; // Store key name if icon
    char seg_mods[MAX_SEGMENTS][64]; // Text part (modifiers or full text)
    
    // First pass: Measure all segments
    int current_char_idx = 0;
    for (int i = 0; i < state->seg_count; i++) {
        int len = state->seg_lengths[i];
        char snippet[128];
        snprintf(snippet, sizeof(snippet), "%.*s", len, state->display_buf + current_char_idx);
        current_char_idx += len;
        
        char mods[64], key[32];
        parse_segment(snippet, mods, key);
        
        cairo_text_extents_t mod_extents;
        cairo_text_extents(cr, mods, &mod_extents);
        
        double w = mod_extents.x_advance;
        
        if (is_icon_key(key)) {
            seg_is_icon[i] = 1;
            strcpy(seg_keys[i], key);
            w += icon_size; // Icon width
        } else {
            seg_is_icon[i] = 0;
            cairo_text_extents_t key_extents;
            cairo_text_extents(cr, key, &key_extents);
            w += key_extents.x_advance;
            // Reconstruct full text for drawing if not icon
            snprintf(seg_mods[i], sizeof(seg_mods[i]), "%s%s", mods, key); // Stored in seg_mods for convenience
        }
        
        if (seg_is_icon[i]) {
            strcpy(seg_mods[i], mods); // Only mods needed
        }
        
        seg_widths[i] = w;
    }
    
    // Calculate fit from end
    double width_so_far = 0;
    for (int i = state->seg_count - 1; i >= 0; i--) {
        if (width_so_far + seg_widths[i] > max_width) {
            start_seg = i + 1;
            break;
        }
        width_so_far += seg_widths[i];
    }
    
    // Draw Phase
    double current_x = state->width - RIGHT_PADDING - width_so_far; 
    if (current_x < PADDING) current_x = PADDING; // Should match max_width logic approx
    
    // Use combo color for the LAST segment if use_combo_color is set
    const double *draw_color = state->text_color;
    
    for (int i = start_seg; i < state->seg_count; i++) {
        // Apply combo color to the last segment only
        if (i == state->seg_count - 1 && state->use_combo_color) {
            draw_color = state->current_combo_color;
            cairo_set_source_rgba(cr, draw_color[0], draw_color[1], draw_color[2], draw_color[3]);
        } else {
            cairo_set_source_rgba(cr, state->text_color[0], state->text_color[1], state->text_color[2], state->text_color[3]);
        }
        
        // Draw Mods/Text
        cairo_move_to(cr, current_x, y_pos);
        cairo_show_text(cr, seg_mods[i]);
        
        cairo_text_extents_t ext;
        cairo_text_extents(cr, seg_mods[i], &ext);
        current_x += ext.x_advance;
        
        // Draw Icon if needed (use combo color if applicable)
        if (seg_is_icon[i]) {
            if (i == state->seg_count - 1 && state->use_combo_color) {
                draw_icon(cr, seg_keys[i], current_x, y_pos, icon_size, state->current_combo_color);
            } else {
                draw_icon(cr, seg_keys[i], current_x, y_pos, icon_size, state->text_color);
            }
            current_x += icon_size;
        }
    }
    
    // Draw mouse click display (bottom of window)
    if (state->mouse.lmb || state->mouse.rmb || state->mouse.mmb) {
        char mouse_info[128];
        char buttons[32] = "";
        
        if (state->mouse.lmb) strcat(buttons, "LMB ");
        if (state->mouse.rmb) strcat(buttons, "RMB ");
        if (state->mouse.mmb) strcat(buttons, "MMB ");
        
        snprintf(mouse_info, sizeof(mouse_info), "%s (%d, %d)", buttons, state->mouse.x, state->mouse.y);
        
        cairo_set_source_rgba(cr, state->text_color[0], state->text_color[1], state->text_color[2], state->text_color[3]);
        cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, state->font_size * 0.5); // Smaller text for mouse
        
        cairo_text_extents_t mouse_ext;
        cairo_text_extents(cr, mouse_info, &mouse_ext);
        double mouse_x = (state->width - mouse_ext.width) / 2.0; // Center
        double mouse_y = state->height - 10;
        
        cairo_move_to(cr, mouse_x, mouse_y);
        cairo_show_text(cr, mouse_info);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(cs);
    munmap(data, size);
    
    wl_surface_attach(state->surface, buffer, 0, 0);
    wl_surface_damage_buffer(state->surface, 0, 0, state->width, state->height);
    wl_surface_commit(state->surface);
    
    if (state->buffer) wl_buffer_destroy(state->buffer);
    state->buffer = buffer;
}
