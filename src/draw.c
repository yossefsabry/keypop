#define _POSIX_C_SOURCE 200809L
#define _USE_MATH_DEFINES
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>
#include <stdio.h>
#include <cairo.h>
#include "draw.h"
#include "shm.h"

void redraw(struct client_state *state) {
    if (!state->surface || !state->window_visible) return;

    const int stride = WINDOW_WIDTH * 4;
    const size_t size = stride * WINDOW_HEIGHT;
    
    int fd = allocate_shm_file(size);
    if (fd == -1) return;
    
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) { close(fd); return; }
    
    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, 
        WINDOW_WIDTH, WINDOW_HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    
    cairo_surface_t *cs = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, WINDOW_WIDTH, WINDOW_HEIGHT, stride);
    cairo_t *cr = cairo_create(cs);
    
    // Clear
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // Background
    const double r = 0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, WINDOW_WIDTH - r, r, r, -M_PI/2, 0);
    cairo_arc(cr, WINDOW_WIDTH - r, WINDOW_HEIGHT - r, r, 0, M_PI/2);
    cairo_arc(cr, r, WINDOW_HEIGHT - r, r, M_PI/2, M_PI);
    cairo_arc(cr, r, r, r, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.60);
    cairo_fill(cr);
    
    // Text
    if (state->display_len > 0) {
        cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 65);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        
        cairo_text_extents_t extents;
        cairo_font_extents_t font_extents;
        const int max_width = WINDOW_WIDTH - PADDING - RIGHT_PADDING;
        
        cairo_font_extents(cr, &font_extents);
        cairo_text_extents(cr, state->display_buf, &extents);
        
        const char *display_text = state->display_buf;
        char overflow_buf[MAX_DISPLAY_LEN + 4];
        
        if (extents.width > max_width) {
            for (size_t i = 1; i < state->display_len; i++) {
                if ((state->display_buf[i] & 0xC0) != 0x80) {
                    snprintf(overflow_buf, sizeof(overflow_buf), "...%s", state->display_buf + i);
                    cairo_text_extents(cr, overflow_buf, &extents);
                    if (extents.width <= max_width) {
                        display_text = overflow_buf;
                        break;
                    }
                }
            }
        }
        
        cairo_text_extents(cr, display_text, &extents);
        double x = WINDOW_WIDTH - RIGHT_PADDING - extents.width;
        if (x < PADDING) x = PADDING;
        double y = (WINDOW_HEIGHT - font_extents.height) / 2.0 + font_extents.ascent + TOP_BOTTOM_PADDING - 7.0;
        
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, display_text);
    }
    
    cairo_destroy(cr);
    cairo_surface_destroy(cs);
    munmap(data, size);
    
    wl_surface_attach(state->surface, buffer, 0, 0);
    wl_surface_damage_buffer(state->surface, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    wl_surface_commit(state->surface);
    
    if (state->buffer) wl_buffer_destroy(state->buffer);
    state->buffer = buffer;
}
