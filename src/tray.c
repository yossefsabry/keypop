#include <libappindicator/app-indicator.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include "tray.h"
#include "window.h"

// Forward declaration
static void on_exit_activate(GtkMenuItem *item, void *data);
static void on_toggle_activate(GtkMenuItem *item, void *data);

struct tray_ctx {
    AppIndicator *indicator;
    GtkWidget *menu;
    GtkWidget *toggle_item;
    struct client_state *state;
};

static struct tray_ctx ctx;

static void update_toggle_label(void) {
    // Checked = Shown, Unchecked = Hidden
    const char *label = ctx.state->overlay_enabled ? "☑ Show && hide" : "☐ Show && hide";
    gtk_menu_item_set_label(GTK_MENU_ITEM(ctx.toggle_item), label);
}

static void on_toggle_activate(GtkMenuItem *item, void *data) {
    (void)item;
    struct client_state *state = data;
    // Toggle the ENABLED state
    state->overlay_enabled = !state->overlay_enabled;
    
    // Update checkbox immediately
    update_toggle_label();
    
    if (!state->overlay_enabled) {
        hide_window(state);
    }
}

static void on_exit_activate(GtkMenuItem *item, void *data) {
    (void)item;
    struct client_state *state = data;
    state->running = 0;
    if (state->loop) g_main_loop_quit(state->loop);
}

int tray_init(struct client_state *state) {
    ctx.state = state;

    int argc = 0;
    char **argv = NULL;
    if (!gtk_init_check(&argc, &argv)) {
        fprintf(stderr, "Failed to init GTK for tray icon\n");
        return -1;
    }

    ctx.indicator = app_indicator_new("keypop-tray", 
                                      "input-keyboard", 
                                      APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    
    app_indicator_set_status(ctx.indicator, APP_INDICATOR_STATUS_ACTIVE);
    
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        char icon_path[1024];
        snprintf(icon_path, sizeof(icon_path), "%s/public", cwd); 
        app_indicator_set_icon_theme_path(ctx.indicator, icon_path);
        app_indicator_set_icon(ctx.indicator, "key_pop"); 
    }

    ctx.menu = gtk_menu_new();
    
    // Show Overlay - Default is CHECKED (☑) because overlay_enabled = 1 (shown)
    ctx.toggle_item = gtk_menu_item_new_with_label("☑ Show && hide");
    g_signal_connect(ctx.toggle_item, "activate", G_CALLBACK(on_toggle_activate), state);
    gtk_menu_shell_append(GTK_MENU_SHELL(ctx.menu), ctx.toggle_item);
    
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Exit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_exit_activate), state);
    gtk_menu_shell_append(GTK_MENU_SHELL(ctx.menu), quit_item);
    
    gtk_widget_show_all(ctx.menu);
    app_indicator_set_menu(ctx.indicator, GTK_MENU(ctx.menu));

    return 0;
}

void tray_destroy(struct client_state *state) {
    (void)state;
}
