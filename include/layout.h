#ifndef MATH_RENDER_LAYOUT_H
#define MATH_RENDER_LAYOUT_H

#include "ast.h"

typedef struct {
    size_t width;
    size_t height;
    size_t baseline;

    char **lines;
} Box;

typedef Box* (*LayoutFunc)(Ast *node);

Box *layout(Ast *node);
void box_free(Box *box);

#endif
