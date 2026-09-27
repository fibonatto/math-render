#include "layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* UTF-8                                                               */
/* ------------------------------------------------------------------ */
static size_t utf8_seq_len(unsigned char lead)
{
    if ((lead & 0x80) == 0x00)
        return 1;
    if ((lead & 0xE0) == 0xC0)
        return 2;
    if ((lead & 0xF0) == 0xE0)
        return 3;
    if ((lead & 0xF8) == 0xF0)
        return 4;

    return 1;
}

static unsigned decode_one_codepoint(const char *text)
{
    if (!text || !text[0])
        return 0;

    unsigned char lead = (unsigned char)text[0];
    size_t len = utf8_seq_len(lead);
    unsigned cp;

    switch (len) {
    case 1:
        cp = lead;
        break;

    case 2:
        if ((text[1] & 0xC0) != 0x80)
            return 0;

        cp = ((lead & 0x1F) << 6) |
             (text[1] & 0x3F);
        break;

    case 3:
        if ((text[1] & 0xC0) != 0x80 ||
            (text[2] & 0xC0) != 0x80)
            return 0;

        cp = ((lead & 0x0F) << 12) |
             ((text[1] & 0x3F) << 6) |
             (text[2] & 0x3F);
        break;

    case 4:
        if ((text[1] & 0xC0) != 0x80 ||
            (text[2] & 0xC0) != 0x80 ||
            (text[3] & 0xC0) != 0x80)
            return 0;

        cp = ((lead & 0x07) << 18) |
             ((text[1] & 0x3F) << 12) |
             ((text[2] & 0x3F) << 6) |
             (text[3] & 0x3F);
        break;

    default:
        return 0;
    }

    if (text[len] != '\0')
        return 0;

    return cp;
}

static char italic_to_ascii_letter(const char *text)
{
    unsigned cp = decode_one_codepoint(text);

    if (cp == 0)
        return 0;

    if (cp >= 0x1D434 && cp <= 0x1D44D)
        return (char)('A' + (cp - 0x1D434));

    if (cp >= 0x1D44E && cp <= 0x1D467)
        return (char)('a' + (cp - 0x1D44E));

    if (cp == 0x210E)
        return 'h';

    return 0;
}

/* ------------------------------------------------------------------ */
/* Box helpers                                                         */
/* ------------------------------------------------------------------ */

static void cell_set_space(Cell *cell)
{
    cell->bytes[0] = ' ';
    cell->bytes[1] = '\0';
}

static void box_set_glyph(
    Box *box,
    size_t x,
    size_t y,
    const char *glyph
)
{
    if (x >= box->width || y >= box->height)
        return;

    strncpy(
        box->lines[y][x].bytes,
        glyph,
        sizeof(box->lines[y][x].bytes) - 1
    );

    box->lines[y][x].bytes[
        sizeof(box->lines[y][x].bytes) - 1
    ] = '\0';
}

static Box *stretch_glyph(
    const char *top,
    const char *fill,
    const char *bottom,
    size_t height
)
{
    if (height < 1)
        height = 1;

    Box *box = box_create(1, height);

    if (!box)
        return NULL;

    if (height == 1) {
        box_set_glyph(box, 0, 0, bottom);
        return box;
    }

    box_set_glyph(box, 0, 0, top);

    for (size_t y = 1; y + 1 < height; y++)
        box_set_glyph(box, 0, y, fill);

    box_set_glyph(box, 0, height - 1, bottom);

    return box;
}

void put_box(Box *dst, const Box *src, size_t x, size_t y)
{
    for (size_t row = 0; row < src->height; row++) {
        size_t dst_y = y + row;

        if (dst_y >= dst->height)
            continue;

        for (size_t col = 0; col < src->width; col++) {
            size_t dst_x = x + col;

            if (dst_x >= dst->width)
                continue;

            dst->lines[dst_y][dst_x] = src->lines[row][col];
        }
    }
}

Box *box_create(size_t width, size_t height)
{
    Box *box = calloc(1, sizeof(Box));

    if (!box)
        return NULL;

    box->width = width;
    box->height = height;

    box->lines = calloc(height, sizeof(Cell *));

    if (!box->lines) {
        free(box);
        return NULL;
    }

    for (size_t y = 0; y < height; y++) {
        box->lines[y] = calloc(width, sizeof(Cell));

        if (!box->lines[y]) {
            for (size_t i = 0; i < y; i++)
                free(box->lines[i]);

            free(box->lines);
            free(box);
            return NULL;
        }

        for (size_t x = 0; x < width; x++)
            cell_set_space(&box->lines[y][x]);
    }

    return box;
}

void box_free(Box *box)
{
    if (!box)
        return;

    for (size_t y = 0; y < box->height; y++)
        free(box->lines[y]);

    free(box->lines);
    free(box);
}

Box *text_box(const char *text)
{
    size_t width = 0;

    for (const char *p = text; *p;) {
        p += utf8_seq_len((unsigned char)*p);
        width++;
    }

    Box *box = box_create(width, 1);

    if (!box)
        return NULL;

    size_t column = 0;

    for (const char *p = text; *p; column++) {
        size_t len = utf8_seq_len((unsigned char)*p);

        if (len > sizeof(box->lines[0][column].bytes) - 1)
            len = 1;

        memcpy(box->lines[0][column].bytes, p, len);
        box->lines[0][column].bytes[len] = '\0';

        p += len;
    }

    return box;
}

/* ------------------------------------------------------------------ */
/* Sequence layout helpers                                             */
/* ------------------------------------------------------------------ */

static int is_single_char_operator(const Ast *node)
{
    if (!node ||
        node->type != AST_TEXT ||
        !node->text ||
        node->text[0] == '\0' ||
        node->text[1] != '\0')
        return 0;

    switch (node->text[0]) {
    case '+':
    case '-':
    case '=':
    case '*':
    case '/':
    case '<':
    case '>':
        return 1;

    default:
        return 0;
    }
}

static size_t sequence_gap(const Ast *left, const Ast *right)
{
    size_t gap = 0;

    if (left &&
        (left->type == AST_SUM ||
         left->type == AST_INT ||
         left->type == AST_PROD))
        gap = 2;

    if (is_single_char_operator(left) ||
        is_single_char_operator(right)) {
        if (gap < 1)
            gap = 1;
    }

    return gap;
}

/* ------------------------------------------------------------------ */
/* Fraction, binomial, and radical layout                              */
/* ------------------------------------------------------------------ */

static Box *fraction_box(Ast *node)
{
    Box *top = layout(node->left);
    Box *bottom = layout(node->right);

    size_t width = top->width > bottom->width
        ? top->width
        : bottom->width;

    if (width < 1)
        width = 1;

    size_t above = top->height;
    size_t below = bottom->height;
    size_t height = above + 1 + below;

    Box *box = box_create(width, height);

    if (!box) {
        box_free(top);
        box_free(bottom);
        return NULL;
    }

    size_t top_x = (width - top->width) / 2;
    size_t bottom_x = (width - bottom->width) / 2;

    put_box(box, top, top_x, 0);

    for (size_t x = 0; x < width; x++)
        box_set_glyph(box, x, above, "—");

    put_box(box, bottom, bottom_x, above + 1);

    box->baseline = above + 1 + bottom->baseline;
    box->axis = above;

    box_free(top);
    box_free(bottom);

    return box;
}

static Box *binom_box(Ast *node)
{
    Box *top = layout(node->left);
    Box *bottom = layout(node->right);

    size_t width = top->width > bottom->width
        ? top->width
        : bottom->width;

    size_t height = top->height + bottom->height;

    Box *stack = box_create(width, height);

    if (!stack) {
        box_free(top);
        box_free(bottom);
        return NULL;
    }

    put_box(stack, top, (width - top->width) / 2, 0);
    put_box(stack, bottom, (width - bottom->width) / 2, top->height);

    Box *left = stretch_glyph("/", "|", "\\", height);
    Box *right = stretch_glyph("\\", "|", "/", height);

    if (!left || !right) {
        box_free(top);
        box_free(bottom);
        box_free(stack);
        box_free(left);
        box_free(right);
        return NULL;
    }

    Box *box = box_create(
        left->width + stack->width + right->width,
        height
    );

    if (!box) {
        box_free(top);
        box_free(bottom);
        box_free(stack);
        box_free(left);
        box_free(right);
        return NULL;
    }

    put_box(box, left, 0, 0);
    put_box(box, stack, left->width, 0);
    put_box(box, right, left->width + stack->width, 0);

    box->baseline = top->height;
    box->axis = box->baseline;

    box_free(top);
    box_free(bottom);
    box_free(stack);
    box_free(left);
    box_free(right);

    return box;
}

static Box *sqrt_box(Ast *node)
{
    Box *radicand = layout(node->left);
    Box *hook = stretch_glyph("/", "│", "√", radicand->height);

    if (!hook) {
        box_free(radicand);
        return NULL;
    }

    size_t width = hook->width + radicand->width;
    size_t height = radicand->height + 1;

    Box *box = box_create(width, height);

    if (!box) {
        box_free(radicand);
        box_free(hook);
        return NULL;
    }

    for (size_t x = hook->width; x < width; x++)
        box_set_glyph(box, x, 0, "─");

    put_box(box, hook, 0, 1);
    put_box(box, radicand, hook->width, 1);

    box->baseline = radicand->baseline + 1;
    box->axis = radicand->axis + 1;

    box_free(radicand);
    box_free(hook);

    return box;
}

/* ------------------------------------------------------------------ */
/* Superscripts and subscripts                                         */
/* ------------------------------------------------------------------ */

static const char *superscript_char(const char *text)
{
    char c;

    if (!text)
        return NULL;

    if (strlen(text) == 1)
        c = text[0];
    else if (!(c = italic_to_ascii_letter(text)))
        return NULL;

    switch (c) {
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
    case 'a': return "ᵃ";
    case 'b': return "ᵇ";
    case 'c': return "ᶜ";
    case 'd': return "ᵈ";
    case 'e': return "ᵉ";
    case 'f': return "ᶠ";
    case 'g': return "ᵍ";
    case 'h': return "ʰ";
    case 'i': return "ⁱ";
    case 'j': return "ʲ";
    case 'k': return "ᵏ";
    case 'l': return "ˡ";
    case 'm': return "ᵐ";
    case 'n': return "ⁿ";
    case 'o': return "ᵒ";
    case 'p': return "ᵖ";
    case 'q': return "ᑫ";
    case 'r': return "ʳ";
    case 's': return "ˢ";
    case 't': return "ᵗ";
    case 'u': return "ᵘ";
    case 'v': return "ᵛ";
    case 'w': return "ʷ";
    case 'x': return "ˣ";
    case 'y': return "ʸ";
    case 'z': return "ᶻ";
    default: return NULL;
    }
}

static const char *subscript_char(const char *text)
{
    char c;

    if (!text)
        return NULL;

    if (strlen(text) == 1)
        c = text[0];
    else if (!(c = italic_to_ascii_letter(text)))
        return NULL;

    switch (c) {
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
    case 'a': return "ₐ";
    case 'e': return "ₑ";
    case 'h': return "ₕ";
    case 'i': return "ᵢ";
    case 'j': return "ⱼ";
    case 'k': return "ₖ";
    case 'l': return "ₗ";
    case 'm': return "ₘ";
    case 'n': return "ₙ";
    case 'o': return "ₒ";
    case 'p': return "ₚ";
    case 'r': return "ᵣ";
    case 's': return "ₛ";
    case 't': return "ₜ";
    case 'u': return "ᵤ";
    case 'v': return "ᵥ";
    case 'x': return "ₓ";
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

        if (!base || !power) {
            box_free(base);
            box_free(power);
            return NULL;
        }

        size_t width = base->width + power->width;
        size_t base_y = power->height > 1
            ? power->height - 1
            : 0;
        size_t height = base_y + base->height;

        Box *box = box_create(width, height);

        if (!box) {
            box_free(base);
            box_free(power);
            return NULL;
        }

        put_box(box, power, base->width, 0);
        put_box(box, base, 0, base_y);

        box->baseline = base_y + base->baseline;

        box_free(base);
        box_free(power);

        return box;
    }

    Box *base = layout(node->left);
    Box *power = layout(node->right);

    if (!base || !power) {
        box_free(base);
        box_free(power);
        return NULL;
    }

    size_t width = base->width + power->width;
    size_t height = base->height + power->height;

    Box *box = box_create(width, height);

    if (!box) {
        box_free(base);
        box_free(power);
        return NULL;
    }

    put_box(box, power, base->width, 0);
    put_box(box, base, 0, power->height);

    box->baseline = power->height + base->baseline;

    box_free(base);
    box_free(power);

    return box;
}

static Box *subscript_box(Ast *node)
{
    const char *sub = NULL;

    if (node->right && node->right->type == AST_TEXT)
        sub = subscript_char(node->right->text);

    if (sub) {
        Box *base = layout(node->left);
        Box *subscript = text_box(sub);

        if (!base || !subscript) {
            box_free(base);
            box_free(subscript);
            return NULL;
        }

        size_t width = base->width + subscript->width;
        size_t height = base->height;

        if (subscript->height > 1)
            height += subscript->height - 1;

        Box *box = box_create(width, height);

        if (!box) {
            box_free(base);
            box_free(subscript);
            return NULL;
        }

        put_box(box, base, 0, 0);
        put_box(box, subscript, base->width, base->height - 1);

        box->baseline = base->baseline;

        box_free(base);
        box_free(subscript);

        return box;
    }

    Box *base = layout(node->left);
    Box *subscript = layout(node->right);

    if (!base || !subscript) {
        box_free(base);
        box_free(subscript);
        return NULL;
    }

    size_t width = base->width + subscript->width;
    size_t height = base->height + subscript->height;

    Box *box = box_create(width, height);

    if (!box) {
        box_free(base);
        box_free(subscript);
        return NULL;
    }

    put_box(box, base, 0, 0);
    put_box(box, subscript, base->width, base->height);

    box->baseline = base->baseline;

    box_free(base);
    box_free(subscript);

    return box;
}

/* ------------------------------------------------------------------ */
/* Sequence layout                                                     */
/* ------------------------------------------------------------------ */

static Box *sequence_box(Ast *node)
{
    if (node->child_count == 0)
        return text_box("");

    Box **boxes = calloc(node->child_count, sizeof(Box *));

    if (!boxes)
        return NULL;

    size_t width = 0;
    size_t above = 0;
    size_t below = 0;

    for (size_t i = 0; i < node->child_count; i++) {
        boxes[i] = layout(node->children[i]);

        if (!boxes[i]) {
            for (size_t j = 0; j < i; j++)
                box_free(boxes[j]);

            free(boxes);
            return NULL;
        }

        width += boxes[i]->width;

        if (i + 1 < node->child_count)
            width += sequence_gap(
                node->children[i],
                node->children[i + 1]
            );

        size_t box_above = boxes[i]->axis;
        size_t box_below =
            boxes[i]->height - boxes[i]->axis - 1;

        if (box_above > above)
            above = box_above;

        if (box_below > below)
            below = box_below;
    }

    size_t height = above + 1 + below;

    Box *box = box_create(width, height);

    if (!box) {
        for (size_t i = 0; i < node->child_count; i++)
            box_free(boxes[i]);

        free(boxes);
        return NULL;
    }

    size_t x = 0;

    for (size_t i = 0; i < node->child_count; i++) {
        size_t y = above - boxes[i]->axis;

        put_box(box, boxes[i], x, y);

        x += boxes[i]->width;

        if (i + 1 < node->child_count)
            x += sequence_gap(
                node->children[i],
                node->children[i + 1]
            );

        box_free(boxes[i]);
    }

    free(boxes);

    box->baseline = above;
    box->axis = above;

    return box;
}

/* ------------------------------------------------------------------ */
/* Large operators                                                     */
/* ------------------------------------------------------------------ */

static Box *stack_limits(Box *sup, Box *op, Box *sub)
{
    size_t width = op->width;

    if (sup && sup->width > width)
        width = sup->width;

    if (sub && sub->width > width)
        width = sub->width;

    size_t height = op->height;

    if (sup)
        height += sup->height;

    if (sub)
        height += sub->height;

    Box *box = box_create(width, height);

    if (!box) {
        box_free(sup);
        box_free(op);
        box_free(sub);
        return NULL;
    }

    size_t y = 0;

    if (sup) {
        put_box(box, sup, (width - sup->width) / 2, y);
        y += sup->height;
        box_free(sup);
    }

    put_box(box, op, (width - op->width) / 2, y);

    box->baseline = y + op->baseline;
    box->axis = y + op->axis;

    y += op->height;
    box_free(op);

    if (sub) {
        put_box(box, sub, (width - sub->width) / 2, y);
        box_free(sub);
    }

    return box;
}

static Box *summation_operator(void)
{
    Box *op = box_create(3, 3);

    if (!op)
        return NULL;

    box_set_glyph(op, 0, 0, "─");
    box_set_glyph(op, 1, 0, "─");
    box_set_glyph(op, 2, 0, "─");
    box_set_glyph(op, 0, 1, "╲");
    box_set_glyph(op, 0, 2, "╱");
    box_set_glyph(op, 1, 2, "⎽");
    box_set_glyph(op, 2, 2, "⎽");

    op->baseline = 2;
    op->axis = 2;

    return op;
}

static Box *integral_operator(void)
{
    Box *op = box_create(1, 4);

    if (!op)
        return NULL;

    box_set_glyph(op, 0, 0, "⌠");
    box_set_glyph(op, 0, 1, "|");
    box_set_glyph(op, 0, 2, "|");
    box_set_glyph(op, 0, 3, "⌡");

    op->baseline = 2;
    op->axis = 2;

    return op;
}

static Box *product_operator(void)
{
    Box *op = box_create(3, 3);

    if (!op)
        return NULL;

    box_set_glyph(op, 0, 0, "─");
    box_set_glyph(op, 1, 0, "─");
    box_set_glyph(op, 2, 0, "─");

    box_set_glyph(op, 0, 1, "│");
    box_set_glyph(op, 2, 1, "│");
    box_set_glyph(op, 0, 2, "│");
    box_set_glyph(op, 2, 2, "│");

    op->baseline = 2;
    op->axis = 2;

    return op;
}

static Box *product_box(Ast *node)
{
    Box *op = product_operator();
    Box *sub = node->left ? layout(node->left) : NULL;
    Box *sup = node->right ? layout(node->right) : NULL;

    if (!op || (node->left && !sub) || (node->right && !sup)) {
        box_free(op);
        box_free(sub);
        box_free(sup);
        return NULL;
    }

    return stack_limits(sup, op, sub);
}

static Box *lim_box(Ast *node)
{
    Box *op = text_box("lim");
    Box *sub = node->left ? layout(node->left) : NULL;

    if (!op || (node->left && !sub)) {
        box_free(op);
        box_free(sub);
        return NULL;
    }

    return stack_limits(NULL, op, sub);
}

static Box *summation_box(Ast *node)
{
    Box *op = summation_operator();
    Box *sub = node->left ? layout(node->left) : NULL;
    Box *sup = node->right ? layout(node->right) : NULL;

    if (!op || (node->left && !sub) || (node->right && !sup)) {
        box_free(op);
        box_free(sub);
        box_free(sup);
        return NULL;
    }

    return stack_limits(sup, op, sub);
}

static Box *integral_box(Ast *node)
{
    Box *op = integral_operator();
    Box *sub = node->left ? layout(node->left) : NULL;
    Box *sup = node->right ? layout(node->right) : NULL;

    if (!op || (node->left && !sub) || (node->right && !sup)) {
        box_free(op);
        box_free(sub);
        box_free(sup);
        return NULL;
    }

    return stack_limits(sup, op, sub);
}

/* ------------------------------------------------------------------ */
/* AST dispatch                                                        */
/* ------------------------------------------------------------------ */

static Box *layout_ast_text(Ast *node)
{
    return text_box(node->text);
}

static const LayoutFunc handlers[] = {
    [AST_TEXT]        = layout_ast_text,
    [AST_FRACTION]    = fraction_box,
    [AST_BINOM]       = binom_box,
    [AST_SUPERSCRIPT] = superscript_box,
    [AST_SUBSCRIPT]   = subscript_box,
    [AST_SEQUENCE]    = sequence_box,
    [AST_SUM]         = summation_box,
    [AST_INT]         = integral_box,
    [AST_SQRT]        = sqrt_box,
    [AST_PROD]        = product_box,
    [AST_LIM]         = lim_box,
};

static const int HANDLERS_COUNT =
    sizeof(handlers) / sizeof(handlers[0]);

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
