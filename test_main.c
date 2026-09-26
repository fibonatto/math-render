#include "layout.h"
#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>

static Ast *mk_text(const char *s)
{
    Ast *n = calloc(1, sizeof(Ast));
    n->type = AST_TEXT;
    n->text = (char *)s;
    return n;
}

static Ast *mk(AstType type, Ast *left, Ast *right)
{
    Ast *n = calloc(1, sizeof(Ast));
    n->type = type;
    n->left = left;
    n->right = right;
    return n;
}

static Ast *mk_seq(Ast **children, size_t count)
{
    Ast *n = calloc(1, sizeof(Ast));
    n->type = AST_SEQUENCE;
    n->children = children;
    n->child_count = count;
    return n;
}

int main(void)
{
    printf("--- \\sum_0^n  (as AST_SUM(left=0,right=n) directly) ---\n");
    Ast *sum = mk(AST_SUM, mk_text("0"), mk_text("n"));
    Box *b1 = layout(sum);
    render(b1);
    box_free(b1);

    printf("\n--- \\sum_0^n  (as generic SUP(SUB(bare-sum,\"0\"),\"n\"), matching the reported bug) ---\n");
    Ast *bare_sum = mk(AST_SUM, NULL, NULL);
    Ast *subbed = mk(AST_SUBSCRIPT, bare_sum, mk_text("0"));
    Ast *supped = mk(AST_SUPERSCRIPT, subbed, mk_text("n"));
    Box *b1b = layout(supped);
    render(b1b);
    box_free(b1b);

    printf("\n--- \\sum_0^n = x  (checking baseline against trailing text) ---\n");
    Ast *children[] = { mk(AST_SUM, mk_text("0"), mk_text("n")), mk_text("=x") };
    Ast *seq = mk_seq(children, 2);
    Box *b2 = layout(seq);
    render(b2);
    box_free(b2);

    printf("\n--- a^2 / b  (fraction + superscript still fine) ---\n");
    Ast *frac = mk(AST_FRACTION, mk(AST_SUPERSCRIPT, mk_text("a"), mk_text("2")), mk_text("b"));
    Box *b3 = layout(frac);
    render(b3);
    box_free(b3);

    printf("\n--- x_1 (subscript fast path) ---\n");
    Ast *sub = mk(AST_SUBSCRIPT, mk_text("x"), mk_text("1"));
    Box *b4 = layout(sub);
    render(b4);
    box_free(b4);

    return 0;
}
