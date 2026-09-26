#include "renderer.h"

#include <stdio.h>
#include <string.h>

void render(Box *box)
{
    if (!box)
        return;

    for (size_t y = 0; y < box->height; y++) {
        /* Trim trailing blank columns, same behavior as the original
         * (which trimmed trailing ' ' bytes off the line buffer). */
        size_t print_upto = 0;
        for (size_t x = 0; x < box->width; x++) {
            if (strcmp(box->lines[y][x].bytes, " ") != 0)
                print_upto = x + 1;
        }

        for (size_t x = 0; x < print_upto; x++)
            fputs(box->lines[y][x].bytes, stdout);

        putchar('\n');
    }
}
