#ifndef LAYOUT_H
#define LAYOUT_H

#include <stddef.h>
#include "ast.h"

typedef struct {
    char bytes[5];
} Cell;

typedef struct Box {
    Cell **lines;     /* lines[y][x] = the glyph at column x of row y   */
    size_t width;     /* width in terminal columns (NOT bytes)          */
    size_t height;     /* number of rows                                 */
    size_t baseline;   /* row that aligns with the surrounding text      */
	size_t axis;
} Box;

typedef Box *(*LayoutFunc)(Ast *node);

Box *layout(Ast *node);
Box *box_create(size_t width, size_t height);
void box_free(Box *box);
void put_box(Box *dst, const Box *src, size_t x, size_t y);
Box *text_box(const char *text);

#endif
