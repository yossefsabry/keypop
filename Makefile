CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
# Use pkg-config for dependencies
PKGS = wayland-client cairo pango pangocairo libinput libudev xkbcommon gtk+-3.0 appindicator3-0.1
# Add -I. to find generated headers in root
CFLAGS += -I. $(shell pkg-config --cflags $(PKGS))
LIBS = $(shell pkg-config --libs $(PKGS)) -lm

SRC = src/main.c src/input.c src/shm.c src/buffer.c src/keys.c src/draw.c src/wl_setup.c src/window.c src/tray.c xdg-shell-protocol.c
OBJ = $(SRC:.c=.o)
TARGET = showmethekey

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Generate protocol code
xdg-shell-protocol.c:
	wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-client-protocol.h:
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $@

# Dependencies
src/main.o: src/main.c src/state.h src/wl_setup.h src/window.h src/keys.h src/draw.h src/tray.h xdg-shell-client-protocol.h
src/input.o: src/input.c src/input.h
src/shm.o: src/shm.c src/shm.h
src/buffer.o: src/buffer.c src/buffer.h src/state.h
src/keys.o: src/keys.c src/keys.h src/buffer.h src/state.h
src/draw.o: src/draw.c src/draw.h src/shm.h src/state.h
src/wl_setup.o: src/wl_setup.c src/wl_setup.h src/state.h
src/window.o: src/window.c src/window.h src/draw.h src/state.h
src/tray.o: src/tray.c src/tray.h src/state.h src/window.h

clean:
	rm -f src/*.o xdg-shell-protocol.o $(TARGET) xdg-shell-protocol.c xdg-shell-client-protocol.h

install: $(TARGET)
	install -D -m 755 $(TARGET) /usr/local/bin/$(TARGET)
