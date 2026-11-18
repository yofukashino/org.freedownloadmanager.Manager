#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

static int real_close_value(int fd) {
    static int (*real_close)(int);

    if (!real_close) real_close = dlsym(RTLD_NEXT, "close");

    if (real_close) return real_close(fd);

    return (int)syscall(SYS_close, fd);
}

static int patch(const char *link) {
    char input[512], output[512];

    const char *replacement = getenv("FDM_WENATIVEHOST_PATH");
    if (!replacement || !*replacement) replacement = "/var/lib/flatpak/exports/bin/org.freedownloadmanager.Manager";

    int file = open(link, O_RDWR);
    if (file <= 0) return real_close_value(file);

    ssize_t len = pread(file, input, sizeof input - 1, 0);

    if (len <= 0 || len >= (ssize_t)sizeof input - 1) return real_close_value(file);

    input[len] = 0;

    char *end = strstr(input, "wenativehost\"");
    char *start = end ? memrchr(input, '"', end - input) : NULL;

    if (start == NULL) return real_close_value(file);

    int n = snprintf(output, sizeof output, "%.*s%s%s", (int)(start + 1 - input), input, replacement, end + strlen("wenativehost"));

    if (n > 0 && n < (int)sizeof output && pwrite(file, output, n, 0) == n) n = ftruncate(file, n);

    return real_close_value(file);
}

int close(int fd) {
    char link[32], name[4096];

    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);

    ssize_t name_size = readlink(link, name, sizeof name - 1);

    if (name_size <= 0 || name_size >= sizeof name - 1) return real_close_value(fd);

    name[name_size] = '\0';
    if (strstr(name, "org.freedownloadmanager.fdm5.cnh.json")) patch(link);

    return real_close_value(fd);
}