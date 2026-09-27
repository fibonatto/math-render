/*
 * test_suite.c -- exercises the full lexer -> parser -> layout -> renderer
 * pipeline against every feature this renderer supports, plus a set of
 * regression tests for bugs found and fixed during development.
 *
 * This is a manual visual test suite, not an automated one: each case
 * prints a label, the input, and the rendered box, so a human can scan
 * the output and confirm it looks right. There are no pass/fail
 * assertions -- the "assertion" is a person's eyes.
 *
 * Part 1 runs full LaTeX-like input strings through parse() -> layout()
 * -> render(), exercising the whole pipeline end to end.
 *
 * Part 2 builds Ast nodes by hand, bypassing the parser entirely, to
 * reach tree shapes the current parser may never produce (but that a
 * future parser change could), and to isolate layout.c bugs from
 * parser.c bugs when debugging a specific report.
 */

#include "ast.h"
#include "layout.h"
#include "parser.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Part 1: integration tests (lexer -> parser -> layout -> renderer)   */
/* ------------------------------------------------------------------ */

static void run(const char *label, const char *input)
{
    printf("=== %s ===\n", label);
    printf("in : %s\n\n", input);

    Ast *ast = parse(input);
    if (!ast) {
        printf("(parse error)\n\n");
        return;
    }

    Box *box = layout(ast);
    if (!box) {
        printf("(layout error)\n\n");
        ast_free(ast);
        return;
    }

    render(box);
    printf("\n");

    box_free(box);
    ast_free(ast);
}

/* ------------------------------------------------------------------ */
/* Part 2: raw-AST tests (bypass the parser, exercise layout.c directly) */
/* ------------------------------------------------------------------ */

static Ast *mk_text(const char *s)
{
    Ast *n = calloc(1, sizeof(Ast));
    n->type = AST_TEXT;
    n->text = strdup(s);
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
    n->child_count = count;
    n->children = malloc(sizeof(Ast *) * count);
    memcpy(n->children, children, sizeof(Ast *) * count);
    return n;
}

static void run_ast(const char *label, Ast *ast)
{
    printf("=== %s ===\n\n", label);

    if (!ast) {
        printf("(null ast)\n\n");
        return;
    }

    Box *box = layout(ast);
    if (!box) {
        printf("(layout error)\n\n");
        ast_free(ast);
        return;
    }

    render(box);
    printf("\n");

    box_free(box);
    ast_free(ast);
}

int main(void)
{
    /* ================================================================
     * 1. Plain text and math italicization
     * ================================================================ */
    run("single letter (italicized)",                 "x");
    run("multi-letter word (sequence of italics)",     "xy");
    run("digits should not italicize",                 "123");
    run("letters and digits mixed",                    "x1y2");

    /* ================================================================
     * 2. User whitespace preserved between terms
     *
     * Regression coverage: adjacent plain terms with no operator
     * between them used to collapse to zero width ("hello world" ->
     * "helloworld"), and a comma used to be swallowed into whichever
     * text run surrounded it ("x_1,x_2" mis-tokenized as one run).
     * ================================================================ */
    run("two words",                                       "hello world");
    run("three loose terms",                               "a b c");
    run("comma must not be absorbed by surrounding text",  "1,000");
    run("comma separating subscripted variables",         "x_1,x_2,x_n");
    run("whitespace plus compact subscript together",     "\\prod_0^n a_k");
    run("whitespace inside a group",                       "\\lim_{x \\to 0} f(x)");
    run("operator with source whitespace (no doubled gap)", "a + b");
    run("operator without source whitespace (same result as above)", "a+b");

    /* ================================================================
     * 3. Binary operators
     * ================================================================ */
    run("operator +",                       "a+b");
    run("operator -",                       "a-b");
    run("operator =",                       "a=b");
    run("operator *",                       "a*b");
    run("operator /",                       "a/b");
    run("operator <",                       "a<b");
    run("operator >",                       "a>b");
    run("chained operators",                "a+b-c=d");
    run("unary minus (known limitation: renders as \"- x\")", "-x");

    /* ================================================================
     * 4. Sub/superscript: compact inline path vs. generic stacked path
     * ================================================================ */
    run("digit subscript (already worked)",              "x_1");
    run("digit superscript (already worked)",              "x^2");
    run("letter subscript -- now compact (fixed italic-vs-unicode bug)", "a_k");
    run("letter superscript -- now compact (fixed italic-vs-unicode bug)", "x^n");
    run("subscript then superscript",                    "x_1^2");
    run("superscript then subscript",                    "x^2_1");
    run("multi-char superscript via group (stacks, not inline)", "x^{10}");
    run("nested superscript (staircase)",                 "x^{y^z}");
    run("two carets in a row (superscript of a superscript)", "x^2^3");
    run("uppercase-letter subscript -- no unicode equivalent, falls back to generic (expected)", "A_T");
    run("original bug report case, isolated",             "(y\\in A_T)");

    /* ================================================================
     * 5. Fractions
     * ================================================================ */
    run("simple fraction",                          "\\frac{a}{b}");
    run("fraction nested in the denominator",       "\\frac{a}{\\frac{b}{c}}");
    run("fraction with superscript in the numerator", "\\frac{a^2}{b}");
    run("\\frac with a single argument (fallback)", "\\frac{a}");
    run("\\frac with no arguments (fallback)",      "\\frac");
    run("\\frac with empty braces",                 "\\frac{}{}");

    /* ================================================================
     * 6. Binomial coefficient
     * ================================================================ */
    run("simple binom",                              "\\binom{n}{k}");
    run("\\binom with a single argument (fallback)", "\\binom{n}");

    /* ================================================================
     * 7. Square root
     * ================================================================ */
    run("simple sqrt",                    "\\sqrt{x}");
    run("nested sqrt",                    "\\sqrt{\\sqrt{x}}");
    run("sqrt of a fraction",             "\\sqrt{\\frac{a}{b}}");
    run("\\sqrt with no argument (fallback)", "\\sqrt");

    /* ================================================================
     * 8. Big operators: sum, integral, product, limit
     * ================================================================ */
    run("sum with an attached term (a_k now renders inline)", "\\sum_0^n a_k");
    run("sum with an attached fraction",              "\\sum_0^n \\frac{a_k}{k!}");
    run("sum with no limits",                         "\\sum");
    run("sum with only a lower limit",                "\\sum_0");
    run("sum with only an upper limit",                "\\sum^n");
    run("integral with an attached term",             "\\int_0^x f(x)");
    run("integral with no limits",                    "\\int");
    run("sum followed by + (must not merge into the operator)", "\\sum_0^n + x");
    run("sum nested in a fraction's numerator",       "\\frac{\\sum_0^n a_k}{n}");
    run("original spacing bug report, full expression",
        "\\sum_0^n \\frac{a_k}{k!} + \\int_0^x \\frac{t^2}{1+t^2}");

    run("product with an attached term",              "\\prod_0^n a_k");
    run("product with i=1 as the lower limit (comma/equals inside a subscript)", "\\prod_{i=1}^n i");
    run("product with no limits",                     "\\prod");

    run("limit with a condition underneath",          "\\lim_{x \\to 0} f(x)");
    run("bare limit (no condition)",                   "\\lim f(x)");
    run("limit next to a fraction (checks axis alignment)", "\\lim_{n\\to\\infty} \\frac{1}{n}");

    /* ================================================================
     * 9. Parentheses, groups, and commas
     * ================================================================ */
    run("simple parentheses",                       "(a+b)");
    run("brace group (no visible delimiter)",       "{a+b}");
    run("nested parentheses",                       "((a+b)*c)");
    run("comma-separated list inside parentheses",  "(a,b,c)");
    run("unclosed parenthesis (resilience)",        "(a+b");
    run("unclosed brace (resilience)",              "{a+b");
    run("empty braces",                             "{}");

    /* ================================================================
     * 10. Greek letters, relations, sets, arrows, calculus symbols
     * ================================================================ */
    run("lowercase greek letters",       "\\alpha + \\beta = \\gamma");
    run("uppercase greek letters",       "\\Gamma \\Delta \\Omega");
    run("relations and operators",       "\\infty \\leq \\geq \\neq \\times \\cdot \\pm");
    run("le/ge/to shorthands (added alongside leq/geq/rightarrow)", "a \\le b \\ge c \\to d");
    run("mid (used in conditional-probability notation)", "P(y \\mid x)");
    run("dots/ldots/cdots",              "x_1,\\ldots,x_n \\dots a \\cdots b");
    run("set theory and logic",          "\\in \\notin \\subset \\cup \\cap \\forall \\exists");
    run("arrows",                        "\\rightarrow \\Rightarrow \\leftrightarrow");
    run("calculus symbols",              "\\partial \\nabla \\hbar");
    run("unknown command (fallback: echoes the command name)", "\\foo");

    /* ================================================================
     * 11. \mathbb and \mathcal
     * ================================================================ */
    run("simple mathbb",                                "\\mathbb{P}");
    run("mathbb without braces, a single atom",         "\\mathbb R");
    run("mathbb followed by a bracket (bracket isn't a special token, expected)", "\\mathbb{E}[X]");
    run("mathcal with a superscript",                    "\\mathcal{X}^*");
    run("lowercase mathcal (no styling -- known limitation, passes through)", "\\mathcal{x}");
    run("full real-world expression: mathbb plus a compound subscript",
        "R(c)=\\mathbb{P}_{y\\sim q_\\theta(\\cdot\\mid c)}(y\\in A_T)");
    run("subset relation with mathcal",                  "A_T \\subseteq \\mathcal{Y}");

    /* ================================================================
     * 12. Composite expressions / stress tests
     * ================================================================ */
    run("quadratic formula",
        "x = \\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}");
    run("binomial theorem",
        "\\binom{n}{k} = \\frac{n!}{k!(n-k)!}");
    run("wide expression (sum+int+sqrt; checks terminal centering/overflow)",
        "\\sum_0^n \\frac{a_k}{k!} + \\int_0^x \\frac{t^2}{1+t^2} + \\sqrt{\\frac{a^2+b^2}{c}} = \\infty");
    run("final composite: product, limit, mathbb, and a letter subscript together",
        "\\lim_{n\\to\\infty} \\prod_{i=1}^n \\mathbb{P}(X_i \\le x)");

    /* ================================================================
     * 13. Resilience (degenerate input)
     * ================================================================ */
    run("empty input",                                "");
    run("whitespace only",                             "   ");
    run("trailing lone backslash (empty command)",     "x\\");
    run("empty command followed by an operator",       "\\+");
    run("underscore with nothing after it",            "x_");
    run("caret with nothing after it",                 "x^");
    run("closing brace with no matching open",         "a}b");
    run("closing parenthesis with no matching open",   "a)b");

    /* Recursion-depth exhaustion is intentionally not exercised here --
     * the output would be enormous and unreadable. Validate it
     * separately instead, e.g.:
     *
     *   python3 -c "print('\\\\sqrt' * 100000)" | math-render
     *
     * Before the depth guard in parse_atom: crashes. After: ugly
     * output (falls back to the literal "\sqrt" text around depth
     * 500), but the process no longer crashes. */

    /* ==================================================================
     * Part 2: raw AST, bypassing the parser -- for layout.c bugs in tree
     * shapes the current parser doesn't produce (but a future one might),
     * and for isolating layout from parser when chasing a specific bug.
     * ================================================================== */

    run_ast("AST_SUM with left/right set directly",
        mk(AST_SUM, mk_text("0"), mk_text("n")));

    run_ast("AST_SUM built generically as SUPERSCRIPT(SUBSCRIPT(sum,0),n)",
        mk(AST_SUPERSCRIPT,
            mk(AST_SUBSCRIPT, mk(AST_SUM, NULL, NULL), mk_text("0")),
            mk_text("n")));

    run_ast("AST_PROD with left/right set directly",
        mk(AST_PROD, mk_text("0"), mk_text("n")));

    run_ast("AST_LIM with no subscript (bare)",
        mk(AST_LIM, NULL, NULL));

    run_ast("AST_LIM with a multi-row subscript (fraction underneath, no superscript)",
        mk(AST_LIM, mk(AST_FRACTION, mk_text("1"), mk_text("n")), NULL));

    {
        Ast *children[] = {
            mk(AST_SUM, mk_text("0"), mk_text("n")),
            mk_text("=x")
        };
        run_ast("sum followed by \"=x\" in a raw sequence (checks baseline)",
            mk_seq(children, 2));
    }

    run_ast("superscript whose base is multi-row (a fraction)",
        mk(AST_SUPERSCRIPT,
            mk(AST_FRACTION, mk_text("a"), mk_text("b")),
            mk_text("2")));

    run_ast("subscript AND superscript, both multi-row",
        mk(AST_SUPERSCRIPT,
            mk(AST_SUBSCRIPT,
                mk(AST_FRACTION, mk_text("a"), mk_text("b")),
                mk(AST_FRACTION, mk_text("c"), mk_text("d"))),
            mk(AST_FRACTION, mk_text("e"), mk_text("f"))));

    {
        Ast *children[] = {
            mk_text("x"),
            mk(AST_SUM, mk_text("0"), mk_text("n"))
        };
        run_ast("sequence ending in a bare big operator (no attached term)",
            mk_seq(children, 2));
    }

    return 0;
}
