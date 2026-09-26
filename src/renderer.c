#include "renderer.h"

#include <stdio.h>
#include <string.h>

void render(Box *box)
{
    if (!box)
        return;

    for (size_t i = 0; i < box->height; i++) {
        size_t length = strlen(box->lines[i]);

        while (length > 0 && box->lines[i][length - 1] == ' ')
            length--;

        fwrite(box->lines[i], 1, length, stdout);
        putchar('\n');
    }
}
