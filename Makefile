CC = gcc
PKG_CONFIG = pkg-config
DEPS = wayland-client cairo pango pangocairo libinput libudev xkbcommon
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I. $(shell $(PKG_CONFIG) --cflags $(DEPS))
LIBS = $(shell $(PKG_CONFIG) --libs $(DEPS))

PROT_DIR = /usr/share/wayland-protocols/stable/xdg-shell
PROT_XML = $(PROT_DIR)/xdg-shell.xml

# Generated files
GEN_SRC = xdg-shell-protocol.c
GEN_HDR = xdg-shell-client-protocol.h

SRC = src/main.c src/input.c $(GEN_SRC)
OBJ = $(SRC:.c=.o)

TARGET = showmethekey

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(GEN_SRC):
	wayland-scanner private-code $(PROT_XML) $@

$(GEN_HDR):
	wayland-scanner client-header $(PROT_XML) $@

# Ensure header is generated before compiling main.c
src/main.o: $(GEN_HDR)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET) $(GEN_SRC) $(GEN_HDR)

install: $(TARGET)
	install -D -m 755 $(TARGET) /usr/local/bin/$(TARGET)
