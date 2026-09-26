#include "layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Box *box_create(size_t width, size_t height)
{
    Box *box = calloc(1, sizeof(Box));

    if (!box)
        return NULL;

    box->width = width;
    box->height = height;

    box->lines = calloc(height, sizeof(char *));

    for (size_t i = 0; i < height; i++) {
        box->lines[i] = calloc(width + 1, 1);
        memset(box->lines[i], ' ', width);
    }

    return box;
}

static Box *text_box(const char *text)
{
    size_t width = strlen(text);

    Box *box = box_create(width, 1);

    if (!box)
        return NULL;

    memcpy(box->lines[0], text, width);

    box->baseline = 0;

    return box;
}

static void put_box(
    Box *dst,
    Box *src,
    size_t x,
    size_t y
)
{
    for (size_t row = 0; row < src->height; row++) {
        size_t src_len = strlen(src->lines[row]);

        if (x >= dst->width || y + row >= dst->height)
            continue;

        size_t available = dst->width - x;

        if (src_len > available)
            src_len = available;

        memcpy(
            dst->lines[y + row] + x,
            src->lines[row],
            src_len
        );
    }
}

static Box *fraction_box(Ast *node)
{
    Box *top = layout(node->left);
    Box *bottom = layout(node->right);

    size_t width = top->width > bottom->width
        ? top->width
        : bottom->width;

    if (width < 1)
        width = 1;

    size_t height =
        top->height +
        1 +
        bottom->height;

    Box *box = box_create(width, height);

    size_t top_x = (width - top->width) / 2;
    size_t bottom_x = (width - bottom->width) / 2;

    put_box(box, top, top_x, 0);

    for (size_t i = 0; i < width; i++)
		box->lines[top->height][i] = '-';

    put_box(
        box,
        bottom,
        bottom_x,
        top->height + 1
    );

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

    if (node->right &&
        node->right->type == AST_TEXT) {
        sup = superscript_char(node->right->text);
    }

    if (sup) {
        Box *base = layout(node->left);
        Box *power = text_box(sup);

        Box *box = box_create(
            base->width + power->width,
            base->height
        );

        put_box(
            box,
            base,
            0,
            0
        );

        put_box(
            box,
            power,
            base->width,
            0
        );

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

    put_box(
        box,
        base,
        0,
        power->height
    );

    put_box(
        box,
        power,
        base->width,
        0
    );

    box->baseline = base->baseline + power->height;

    box_free(base);
    box_free(power);

    return box;
}

static Box *sequence_box(Ast *node)
{
    if (node->child_count == 0)
        return text_box("");

    Box **boxes = calloc(
        node->child_count,
        sizeof(Box *)
    );

    size_t width = 0;
    size_t above = 0;
    size_t below = 0;

    for (size_t i = 0; i < node->child_count; i++) {
        boxes[i] = layout(node->children[i]);

        width += boxes[i]->width;

        size_t box_above = boxes[i]->baseline;
        size_t box_below =
            boxes[i]->height - boxes[i]->baseline - 1;

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

static Box* layout_ast_text(Ast *node) { return text_box(node->text); }
static Box* layout_sum(Ast *node)      { return text_box("∑"); }
static Box* layout_int(Ast *node)      { return text_box("∫"); }

static const LayoutFunc handlers[] = {
    [AST_TEXT]        = layout_ast_text,
    [AST_FRACTION]    = fraction_box,
    [AST_SUPERSCRIPT] = superscript_box,
    [AST_SUBSCRIPT]   = sequence_box,
    [AST_SEQUENCE]    = sequence_box,
    [AST_SUM]         = layout_sum,
    [AST_INT]         = layout_int,
};

static const int HANDLERS_COUNT = sizeof(handlers) / sizeof(handlers[0]);

Box *layout(Ast *node)
{
    if (!node)
        return text_box("");

    if (node->type >= 0 && node->type < HANDLERS_COUNT) {
        LayoutFunc func = handlers[node->type];
        if (func) {
            return func(node);
        }
    }

    return text_box("");
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
