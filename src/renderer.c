#define _DEFAULT_SOURCE

#include "renderer.h"

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static size_t terminal_width(void)
{
    struct winsize window;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 &&
        window.ws_col > 0)
        return window.ws_col;

    return 80;
}

static size_t content_width(const Box *box, size_t row)
{
    size_t width = 0;

    for (size_t x = 0; x < box->width; x++) {
        if (strcmp(box->lines[row][x].bytes, " ") != 0)
            width = x + 1;
    }

    return width;
}

void render(Box *box)
{
    if (!box)
        return;

    size_t width = terminal_width();
    size_t left_pad = box->width < width
        ? (width - box->width) / 2
        : 0;

    for (size_t y = 0; y < box->height; y++) {
        size_t print_width = content_width(box, y);

        for (size_t i = 0; i < left_pad; i++)
            putchar(' ');

        for (size_t x = 0; x < print_width; x++)
            fputs(box->lines[y][x].bytes, stdout);

        putchar('\n');
    }
}
