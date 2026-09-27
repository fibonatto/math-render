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
    if ((lead & 0x80) == 0x00) return 1; /* 0xxxxxxx */
    if ((lead & 0xE0) == 0xC0) return 2; /* 110xxxxx */
    if ((lead & 0xF0) == 0xE0) return 3; /* 1110xxxx */
    if ((lead & 0xF8) == 0xF0) return 4; /* 11110xxx */
    return 1;
}

/* ------------------------------------------------------------------ */
/* Box primitives                                                       */
/* ------------------------------------------------------------------ */

static int is_single_char_operator(const Ast *n)
{
    if (!n || n->type != AST_TEXT || !n->text || n->text[0] == '\0' || n->text[1] != '\0')
        return 0;

    switch (n->text[0]) {
    case '+': case '-': case '=': case '*': case '/': case '<': case '>':
        return 1;
    default:
        return 0;
    }
}

static size_t sequence_gap(const Ast *left, const Ast *right)
{
    size_t gap = 0;

    if (left && (left->type == AST_SUM || left->type == AST_INT))
        gap = 2;

    if (is_single_char_operator(left) || is_single_char_operator(right))
        if (gap < 1)
            gap = 1;

    return gap;
}

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
	box->axis = 0;

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
	box->axis = 0;
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

/* Stacks top/fill/bottom into a single-column Box `height` rows tall:
 * `top` at row 0, `bottom` at the last row, `fill` repeated for every
 * row in between (zero or more of them). For a height of 1 there's no
 * room for a separate top and bottom, so `bottom` alone is used --
 * which is exactly the right degenerate case for \sqrt{x} on a single
 * line (the "√" hook with no extra ascender above it).
 *
 * This is the piece \binom's parentheses and \sqrt's radical both
 * need -- a symbol that grows to match its content's height -- kept
 * as one function instead of two near-copies so a future \left(...\right)
 * or similar has somewhere to plug in too. */
static Box *stretch_glyph(const char *top, const char *fill, const char *bottom, size_t height)
{
    if (height < 1)
        height = 1;

    Box *box = box_create(1, height);

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

static Box *binom_box(Ast *node)
{
    Box *top = layout(node->left);
    Box *bottom = layout(node->right);

    size_t inner_width = top->width > bottom->width ? top->width : bottom->width;
    size_t height = top->height + bottom->height;

    /* Unlike \frac, there's no divider bar between the two terms. */
    Box *stack = box_create(inner_width, height);
    put_box(stack, top, (inner_width - top->width) / 2, 0);
    put_box(stack, bottom, (inner_width - bottom->width) / 2, top->height);

    Box *lparen = stretch_glyph("/", "|", "\\", height);
    Box *rparen = stretch_glyph("\\", "|", "/", height);

    Box *box = box_create(lparen->width + stack->width + rparen->width, height);
    put_box(box, lparen, 0, 0);
    put_box(box, stack, lparen->width, 0);
    put_box(box, rparen, lparen->width + stack->width, 0);

    /* Same convention as fraction_box: baseline lands on the row where
     * the bottom term starts (there, that row holds the divider bar;
     * here, it's the bottom term's own first row). */
    box->baseline = top->height;

    box_free(top);
    box_free(bottom);
    box_free(stack);
    box_free(lparen);
    box_free(rparen);

    return box;
}

static Box *sqrt_box(Ast *node)
{
    Box *radicand = layout(node->left);

    Box *hook = stretch_glyph("/", "│", "√", radicand->height);

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

    box_free(radicand);
    box_free(hook);

    return box;
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

static unsigned decode_one_codepoint(const char *text)
{
    if (!text || !text[0])
        return 0;

    unsigned char lead = (unsigned char) text[0];
    size_t len = utf8_seq_len(lead);
    unsigned cp;

    switch (len) {
    case 1:
        cp = lead;
        break;
    case 2:
        if ((text[1] & 0xC0) != 0x80) return 0;
        cp = ((lead & 0x1F) << 6) | (text[1] & 0x3F);
        break;
    case 3:
        if ((text[1] & 0xC0) != 0x80 || (text[2] & 0xC0) != 0x80) return 0;
        cp = ((lead & 0x0F) << 12) | ((text[1] & 0x3F) << 6) | (text[2] & 0x3F);
        break;
    case 4:
        if ((text[1] & 0xC0) != 0x80 || (text[2] & 0xC0) != 0x80 || (text[3] & 0xC0) != 0x80) return 0;
        cp = ((lead & 0x07) << 18) | ((text[1] & 0x3F) << 12) | ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
        break;
    default:
        return 0;
    }

    if (text[len] != '\0') /* sobrou mais coisa depois -- não é 1 char só */
        return 0;

    return cp;
}

static char italic_to_ascii_letter(const char *text)
{
    unsigned cp = decode_one_codepoint(text);

    if (cp == 0)
        return 0;

    if (cp >= 0x1D434 && cp <= 0x1D44D)   /* Mathematical Italic Capital A-Z */
        return (char) ('A' + (cp - 0x1D434));

    if (cp >= 0x1D44E && cp <= 0x1D467)   /* Mathematical Italic Small a-z */
        return (char) ('a' + (cp - 0x1D44E));

    if (cp == 0x210E)                     /* itálico de "h" (Planck constant) */
        return 'h';

    return 0;
}

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

static Box *superscript_box(Ast *node)
{
    const char *sup = NULL;

    if (node->right && node->right->type == AST_TEXT)
        sup = superscript_char(node->right->text);

    if (sup) {
        Box *base = layout(node->left);
        Box *power = text_box(sup);

        size_t width = base->width + power->width;

        /*
         * Keep the base where it was. The superscript occupies the
         * upper part of the box.
         */
        size_t height = base->height;
        if (power->height > 1)
            height += power->height - 1;

        Box *box = box_create(width, height);

        size_t power_y = 0;
        size_t base_y = power->height > 1
            ? power->height - 1
            : 0;

        put_box(box, power, base->width, power_y);
        put_box(box, base, 0, base_y);

        box->baseline = base_y + base->baseline;

        box_free(base);
        box_free(power);

        return box;
    }

    Box *base = layout(node->left);
    Box *power = layout(node->right);

    size_t width = base->width + power->width;
    size_t height = base->height + power->height;

    Box *box = box_create(width, height);

    put_box(box, power, base->width, 0);
    put_box(box, base, 0, power->height);

    box->baseline = power->height + base->baseline;

    box_free(base);
    box_free(power);

    return box;
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

static Box *subscript_box(Ast *node)
{
    const char *sub = NULL;

    if (node->right && node->right->type == AST_TEXT)
        sub = subscript_char(node->right->text);

    if (sub) {
        Box *base = layout(node->left);
        Box *power = text_box(sub);

        size_t width = base->width + power->width;
        size_t height = base->height;

        if (power->height > 1)
            height += power->height - 1;

        Box *box = box_create(width, height);

        put_box(box, base, 0, 0);

        /*
         * Put the subscript on the bottom row while preserving the
         * original baseline of the base.
         */
        size_t sub_y = base->height - 1;

        put_box(box, power, base->width, sub_y);

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
            width += sequence_gap(node->children[i], node->children[i + 1]);

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
            x += sequence_gap(node->children[i], node->children[i + 1]);

        box_free(boxes[i]);
    }

    free(boxes);

    /*
     * The sequence's own mathematical axis is the same
     * horizontal line used to align all of its children.
     *
     * This is especially important for nested sequences such as:
     *
     *     SEQUENCE
     *     ├── SUM
     *     └── FRACTION
     */
    box->baseline = above;
    box->axis = above;

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
        put_box(
            box,
            sup,
            (width - sup->width) / 2,
            y
        );

        y += sup->height;
        box_free(sup);
    }

    put_box(
        box,
        op,
        (width - op->width) / 2,
        y
    );

    box->baseline = y + op->baseline;
    box->axis = y + op->axis;

    y += op->height;
    box_free(op);

    if (sub) {
        put_box(
            box,
            sub,
            (width - sub->width) / 2,
            y
        );

        box_free(sub);
    }

    return box;
}
/* Hand-drawn multi-row operators, built entirely from ASCII and common
 * box-drawing characters so nothing depends on a font's handling of
 * rare math-extension glyphs (which is what made the earlier ⎲/⎳
 * attempt look broken in practice).
 *
 *   ───          ⌠
 *   ╲            |
 *   ╱⎽⎽          |
 *                ⌡
 *
 * Baseline is the row surrounding text should align with: for the sum
 * sign that's the diagonal stroke (its "waist"); for the integral,
 * the lower of the two vertical-bar rows. Both are just a starting
 * pick -- easy to move by changing the single ->baseline assignment
 * below if it doesn't look right against real content. */
static Box *summation_operator(void)
{
    Box *op = box_create(3, 3);

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

    box_set_glyph(op, 0, 0, "⌠");
    box_set_glyph(op, 0, 1, "|");
    box_set_glyph(op, 0, 2, "|");
    box_set_glyph(op, 0, 3, "⌡");

    op->baseline = 2;
    op->axis = 2;

    return op;
}

static Box *summation_box(Ast *node)
{
    Box *op = summation_operator();
    Box *sub = node->left
        ? layout(node->left)
        : NULL;
    Box *sup = node->right
        ? layout(node->right)
        : NULL;

    return stack_limits(sup, op, sub);
}

static Box *integral_box(Ast *node)
{
    Box *op = integral_operator();
    Box *sub = node->left
        ? layout(node->left)
        : NULL;
    Box *sup = node->right
        ? layout(node->right)
        : NULL;

    return stack_limits(sup, op, sub);
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
