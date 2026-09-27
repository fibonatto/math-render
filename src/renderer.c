/* Needed under strict -std=c11 (rather than gnu11) for sys/ioctl.h to
 * expose struct winsize/TIOCGWINSZ -- must come before any header. */
#define _DEFAULT_SOURCE

#include "renderer.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

static size_t terminal_width(void)
{
    struct winsize w;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return w.ws_col;

    return 80; /* stdout isn't a tty (piped, redirected to a file, ...) */
}

void render(Box *box)
{
    if (!box)
        return;

    size_t width = terminal_width();
    size_t left_pad = box->width < width ? (width - box->width) / 2 : 0;

    for (size_t y = 0; y < box->height; y++) {
        /* Trim trailing blank columns, same behavior as the original
         * (which trimmed trailing ' ' bytes off the line buffer). */
        size_t print_upto = 0;
        for (size_t x = 0; x < box->width; x++) {
            if (strcmp(box->lines[y][x].bytes, " ") != 0)
                print_upto = x + 1;
        }

        for (size_t i = 0; i < left_pad; i++)
            putchar(' ');

        for (size_t x = 0; x < print_upto; x++)
            fputs(box->lines[y][x].bytes, stdout);

        putchar('\n');
    }
}
