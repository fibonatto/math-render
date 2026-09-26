#include "layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* UTF-8 helpers                                                        */
/* ------------------------------------------------------------------ */

/* Length in bytes of the UTF-8 sequence starting with `lead`. Falls
 * back to 1 for a stray continuation byte so callers always make
 * forward progress instead of looping forever on malformed input. */
static size_t utf8_seq_len(unsigned char lead)
{
    if ((lead & 0x80) == 0x00) return 1;   /* 0xxxxxxx */
    if ((lead & 0xE0) == 0xC0) return 2;   /* 110xxxxx */
    if ((lead & 0xF0) == 0xE0) return 3;   /* 1110xxxx */
    if ((lead & 0xF8) == 0xF0) return 4;   /* 11110xxx */
    return 1;
}

/* ------------------------------------------------------------------ */
/* Box primitives                                                       */
/* ------------------------------------------------------------------ */

static void cell_set_space(Cell *cell)
{
    cell->bytes[0] = ' ';
    cell->bytes[1] = '\0';
}

Box *box_create(size_t width, size_t height)
{
    Box *box = calloc(1, sizeof(Box));
    if (!box)
        return NULL;

    box->width = width;
    box->height = height;
    box->baseline = 0;

    box->lines = calloc(height, sizeof(Cell *));
    if (!box->lines) {
        free(box);
        return NULL;
    }

    for (size_t y = 0; y < height; y++) {
        box->lines[y] = calloc(width, sizeof(Cell));
        for (size_t x = 0; x < width; x++)
            cell_set_space(&box->lines[y][x]);
    }

    return box;
}

void box_free(Box *box)
{
    if (!box)
        return;

    for (size_t i = 0; i < box->height; i++)
        free(box->lines[i]);

    free(box->lines);
    free(box);
}

Box *text_box(const char *text)
{
    /* Pass 1: count codepoints, i.e. visual columns. This is the fix --
     * the original code used strlen() (byte count) as the visual width,
     * which is only correct for pure ASCII. Every multi-byte glyph in
     * this renderer (\sum's "∑", the sub/superscript digits) made boxes
     * wider than they actually are on screen, and every offset computed
     * from that width inherited the error. */
    size_t width = 0;
    for (const char *p = text; *p; )
        p += utf8_seq_len((unsigned char)*p), width++;

    Box *box = box_create(width, 1);
    if (!box)
        return NULL;

    size_t col = 0;
    for (const char *p = text; *p; col++) {
        size_t n = utf8_seq_len((unsigned char)*p);
        if (n > sizeof(box->lines[0][col].bytes) - 1)
            n = 1; /* defensive; never true for valid UTF-8 */
        memcpy(box->lines[0][col].bytes, p, n);
        box->lines[0][col].bytes[n] = '\0';
        p += n;
    }

    box->baseline = 0;
    return box;
}

void put_box(Box *dst, const Box *src, size_t x, size_t y)
{
    /* Copying whole Cells (not raw bytes at a byte offset that used to
     * assume "4 bytes per column") means this is correct regardless of
     * how many bytes any given glyph encodes to, and it's now bounds
     * checked -- the original had none, so a wide child box placed near
     * a parent's right edge could write past the destination buffer. */
    for (size_t row = 0; row < src->height; row++) {
        size_t dst_y = y + row;
        if (dst_y >= dst->height)
            continue;

        for (size_t colu = 0; colu < src->width; colu++) {
            size_t dst_x = x + colu;
            if (dst_x >= dst->width)
                continue;

            dst->lines[dst_y][dst_x] = src->lines[row][colu];
        }
    }
}

static void box_set_glyph(Box *box, size_t x, size_t y, const char *glyph)
{
    if (x >= box->width || y >= box->height)
        return;
    strncpy(box->lines[y][x].bytes, glyph, sizeof(box->lines[y][x].bytes) - 1);
    box->lines[y][x].bytes[sizeof(box->lines[y][x].bytes) - 1] = '\0';
}

/* ------------------------------------------------------------------ */
/* Layout handlers                                                       */
/* ------------------------------------------------------------------ */

static Box *fraction_box(Ast *node)
{
    Box *top = layout(node->left);
    Box *bottom = layout(node->right);

    size_t width = top->width > bottom->width ? top->width : bottom->width;
    if (width < 1)
        width = 1;

    size_t height = top->height + 1 + bottom->height;

    Box *box = box_create(width, height);

    size_t top_x = (width - top->width) / 2;
    size_t bottom_x = (width - bottom->width) / 2;

    put_box(box, top, top_x, 0);

    for (size_t x = 0; x < width; x++)
        box_set_glyph(box, x, top->height, "-");

    put_box(box, bottom, bottom_x, top->height + 1);

    box->baseline = top->height + 1;

    box_free(top);
    box_free(bottom);

    return box;
}

static const char *superscript_char(const char *text)
{
    if (!text || strlen(text) != 1)
        return NULL;

    switch (text[0]) {
    case '0': return "⁰";
    case '1': return "¹";
    case '2': return "²";
    case '3': return "³";
    case '4': return "⁴";
    case '5': return "⁵";
    case '6': return "⁶";
    case '7': return "⁷";
    case '8': return "⁸";
    case '9': return "⁹";
    case '+': return "⁺";
    case '-': return "⁻";
    case '=': return "⁼";
    case '(': return "⁽";
    case ')': return "⁾";
    default: return NULL;
    }
}

static Box *superscript_box(Ast *node)
{
    const char *sup = NULL;

    if (node->right && node->right->type == AST_TEXT)
        sup = superscript_char(node->right->text);

    if (sup) {
        Box *base = layout(node->left);
        Box *power = text_box(sup);

        Box *box = box_create(base->width + power->width, base->height);

        put_box(box, base, 0, 0);
        put_box(box, power, base->width, 0);

        box->baseline = base->baseline;

        box_free(base);
        box_free(power);

        return box;
    }

    Box *base = layout(node->left);
    Box *power = layout(node->right);

    size_t width = base->width + power->width;
    size_t height = base->height + power->height;

    Box *box = box_create(width, height);

    put_box(box, base, 0, power->height);
    put_box(box, power, base->width, 0);

    box->baseline = base->baseline + power->height;

    box_free(base);
    box_free(power);

    return box;
}

static const char *subscript_char(const char *text)
{
    if (!text || strlen(text) != 1)
        return NULL;

    switch (text[0]) {
    case '0': return "₀";
    case '1': return "₁";
    case '2': return "₂";
    case '3': return "₃";
    case '4': return "₄";
    case '5': return "₅";
    case '6': return "₆";
    case '7': return "₇";
    case '8': return "₈";
    case '9': return "₉";
    case '+': return "₊";
    case '-': return "₋";
    case '=': return "₌";
    case '(': return "₍";
    case ')': return "₎";
    default: return NULL;
    }
}

static Box *subscript_box(Ast *node)
{
    const char *sub = NULL;

    if (node->right && node->right->type == AST_TEXT)
        sub = subscript_char(node->right->text);

    if (sub) {
        Box *base = layout(node->left);
        Box *power = text_box(sub);

        Box *box = box_create(base->width + power->width, base->height);

        /* Subscript goes on the base's BOTTOM row, not row 0. For a
         * 1-row base (plain letters/digits) those are the same row, so
         * this looked right for years -- it only broke once a base
         * (the \sum operator) got taller than 1 row. */
        size_t sub_row = base->height - 1;

        put_box(box, base, 0, 0);
        put_box(box, power, base->width, sub_row);

        box->baseline = base->baseline;

        box_free(base);
        box_free(power);

        return box;
    }

    Box *base = layout(node->left);
    Box *sub_box = layout(node->right);

    size_t width = base->width + sub_box->width;
    size_t height = base->height + sub_box->height;

    Box *box = box_create(width, height);

    put_box(box, base, 0, 0);
    put_box(box, sub_box, base->width, base->height);

    box->baseline = base->baseline;

    box_free(base);
    box_free(sub_box);

    return box;
}

static Box *sequence_box(Ast *node)
{
    if (node->child_count == 0)
        return text_box("");

    Box **boxes = calloc(node->child_count, sizeof(Box *));

    size_t width = 0;
    size_t above = 0;
    size_t below = 0;

    for (size_t i = 0; i < node->child_count; i++) {
        boxes[i] = layout(node->children[i]);

        width += boxes[i]->width;

        size_t box_above = boxes[i]->baseline;
        size_t box_below = boxes[i]->height - boxes[i]->baseline - 1;

        if (box_above > above)
            above = box_above;
        if (box_below > below)
            below = box_below;
    }

    size_t height = above + 1 + below;

    Box *box = box_create(width, height);

    size_t x = 0;

    for (size_t i = 0; i < node->child_count; i++) {
        size_t y = above - boxes[i]->baseline;

        put_box(box, boxes[i], x, y);

        x += boxes[i]->width;

        box_free(boxes[i]);
    }

    free(boxes);

    box->baseline = above;

    return box;
}

static Box *layout_ast_text(Ast *node) { return text_box(node->text); }

/*
 * Stacks an optional superscript, a (possibly multi-row) operator glyph,
 * and an optional subscript into one box, centering each row on the
 * widest one. This replaces the old apply_limits(), which rebuilt the
 * box twice (once per limit) using the same width/height math -- same
 * behavior, expressed once, and now able to take a multi-row operator
 * (needed for the two-glyph "⎲ / ⎳" summation sign below) instead of
 * assuming the operator is always a single row.
 *
 * Takes ownership of sup/op/sub: all non-NULL boxes passed in are freed
 * before this returns.
 */
static Box *stack_limits(Box *sup, Box *op, Box *sub)
{
    size_t width = op->width;
    if (sup && sup->width > width) width = sup->width;
    if (sub && sub->width > width) width = sub->width;

    size_t height = op->height;
    if (sup) height += sup->height;
    if (sub) height += sub->height;

    Box *box = box_create(width, height);

    size_t y = 0;

    if (sup) {
        put_box(box, sup, (width - sup->width) / 2, y);
        y += sup->height;
        box_free(sup);
    }

    put_box(box, op, (width - op->width) / 2, y);
    box->baseline = y + op->baseline;
    y += op->height;
    box_free(op);

    if (sub)
        put_box(box, sub, (width - sub->width) / 2, y);
    box_free(sub);

    return box;
}

/* The classic ∑ character is 1 row and, worse for us, 3 bytes -- exactly
 * the kind of multi-byte glyph that triggered the original bug. Per your
 * request we build the tall sum sign from its two Unicode halves
 * instead, stacked with no blank row between them. Its baseline is the
 * bottom row, matching how a single-row glyph's baseline sits at its
 * own row -- so text after the \sum lines up with "⎳", not "⎲". */
static Box *summation_operator(void)
{
    Box *op = box_create(1, 2);
    box_set_glyph(op, 0, 0, "⎲");
    box_set_glyph(op, 0, 1, "⎳");
    op->baseline = 1;
    return op;
}

static Box *summation_box(Ast *node)
{
    Box *op = summation_operator();
    Box *sub = node->left  ? layout(node->left)  : NULL;
    Box *sup = node->right ? layout(node->right) : NULL;
    return stack_limits(sup, op, sub);
}

static Box *integral_box(Ast *node)
{
    Box *op = text_box("∫");
    Box *sub = node->left  ? layout(node->left)  : NULL;
    Box *sup = node->right ? layout(node->right) : NULL;
    return stack_limits(sup, op, sub);
}

static const LayoutFunc handlers[] = {
    [AST_TEXT]        = layout_ast_text,
    [AST_FRACTION]    = fraction_box,
    [AST_SUPERSCRIPT] = superscript_box,
    [AST_SUBSCRIPT]   = subscript_box,
    [AST_SEQUENCE]    = sequence_box,
    [AST_SUM]         = summation_box,
    [AST_INT]         = integral_box,
};

static const int HANDLERS_COUNT = sizeof(handlers) / sizeof(handlers[0]);

Box *layout(Ast *node)
{
    if (!node)
        return text_box("");

    if (node->type >= 0 && node->type < HANDLERS_COUNT) {
        LayoutFunc func = handlers[node->type];
        if (func)
            return func(node);
    }

    return text_box("");
}
