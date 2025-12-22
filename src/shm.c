#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include "shm.h"

static int create_shm_file(void) {
    static unsigned int counter = 0;
    char name[32];
    snprintf(name, sizeof(name), "/wl_shm-%u-%u", getpid(), counter++);
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) shm_unlink(name);
    return fd;
}

int allocate_shm_file(size_t size) {
    int fd = create_shm_file();
    if (fd < 0) return -1;
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}
