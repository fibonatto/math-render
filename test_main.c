#include "ast.h"
#include "layout.h"
#include "parser.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Parte 1: testes de integração (lexer -> parser -> layout -> renderer) */
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
/* Parte 2: testes de AST cru (bypassa o parser, ataca o layout.c direto) */
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
        printf("(ast nulo)\n\n");
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
    /* ---------------- texto básico / itálico matemático ---------------- */
    run("letra isolada (itálico)",                          "x");
    run("palavra multi-letra (vira sequência de itálicos)",  "xy");
    run("dígitos não devem italicizar",                      "123");
    run("letras e dígitos misturados",                       "x1y2");
    run("duas palavras (o espaço do usuário é preservado?)", "hello world");

    /* ---------------- operadores binários ---------------- */
    run("operador +",                       "a+b");
    run("operador -",                       "a-b");
    run("operador =",                       "a=b");
    run("operador *",                       "a*b");
    run("operador /",                       "a/b");
    run("operador <",                       "a<b");
    run("operador >",                       "a>b");
    run("cadeia de operadores",             "a+b-c=d");
    run("menos unário (limitação conhecida: vira \"- x\")", "-x");

    /* ---------------- sub/superscript genérico ---------------- */
    run("superscript simples",                        "x^2");
    run("subscript simples",                          "x_1");
    run("sub depois super",                           "x_1^2");
    run("super depois sub",                           "x^2_1");
    run("superscript multi-char via grupo",           "x^{10}");
    run("superscript aninhado",                       "x^{y^z}");
    run("dois carets em sequência (super de super)",  "x^2^3");

    /* ---------------- frações ---------------- */
    run("fração simples",                        "\\frac{a}{b}");
    run("fração aninhada no denominador",        "\\frac{a}{\\frac{b}{c}}");
    run("fração com superscript no numerador",   "\\frac{a^2}{b}");
    run("\\frac com um só argumento (fallback)", "\\frac{a}");
    run("\\frac sem nenhum argumento (fallback)","\\frac");

    /* ---------------- binom ---------------- */
    run("binom simples",                          "\\binom{n}{k}");
    run("\\binom com um só argumento (fallback)", "\\binom{n}");

    /* ---------------- sqrt ---------------- */
    run("sqrt simples",                    "\\sqrt{x}");
    run("sqrt aninhado",                   "\\sqrt{\\sqrt{x}}");
    run("sqrt de fração",                  "\\sqrt{\\frac{a}{b}}");
    run("\\sqrt sem argumento (fallback)", "\\sqrt");

    /* ---------------- sum / int (operadores grandes) ---------------- */
    run("sum com termo colado",                    "\\sum_0^n a_k");
    run("sum + fração colada",                     "\\sum_0^n \\frac{a_k}{k!}");
    run("sum sem limites",                         "\\sum");
    run("sum só com limite inferior",              "\\sum_0");
    run("sum só com limite superior",              "\\sum^n");
    run("int com termo colado",                    "\\int_0^x f(x)");
    run("int sem limites",                         "\\int");
    run("sum seguido de + (não deveria colar)",    "\\sum_0^n + x");
    run("sum dentro do numerador de uma fração",   "\\frac{\\sum_0^n a_k}{n}");
    run("caso original do bug report",
        "\\sum_0^n \\frac{a_k}{k!} + \\int_0^x \\frac{t^2}{1+t^2}");

    /* ---------------- parênteses e grupos ---------------- */
    run("parênteses simples",                  "(a+b)");
    run("grupo em chaves (sem símbolo visível)","{a+b}");
    run("parênteses aninhados",                "((a+b)*c)");
    run("parêntese sem fechar (resiliência)",  "(a+b");
    run("chave sem fechar (resiliência)",      "{a+b");

    /* ---------------- letras gregas e símbolos ---------------- */
    run("gregas minúsculas",             "\\alpha + \\beta = \\gamma");
    run("gregas maiúsculas",             "\\Gamma \\Delta \\Omega");
    run("relações e operadores",         "\\infty \\leq \\geq \\neq \\times \\cdot \\pm");
    run("conjuntos e lógica",            "\\in \\notin \\subset \\cup \\cap \\forall \\exists");
    run("setas",                         "\\rightarrow \\Rightarrow \\leftrightarrow");
    run("cálculo",                       "\\partial \\nabla \\hbar");
    run("comando desconhecido (fallback: ecoa o nome)", "\\foo");

    /* ---------------- comandos comentados / não implementados ---------------- */
    run("\\prod (fora da tabela -- vira texto literal \"prod\")", "\\prod_0^n a_k");
    run("\\lim (fora da tabela -- vira texto literal \"lim\")",   "\\lim_{x \\to 0} f(x)");

    /* ---------------- expressões compostas / stress test ---------------- */
    run("fórmula de Bhaskara",
        "x = \\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}");
    run("teorema binomial",
        "\\binom{n}{k} = \\frac{n!}{k!(n-k)!}");
    run("expressão larga (checa centralização/overflow no terminal)",
        "\\sum_0^n \\frac{a_k}{k!} + \\int_0^x \\frac{t^2}{1+t^2} + \\sqrt{\\frac{a^2+b^2}{c}} = \\infty");

    /* ==================================================================
     * Parte 2: AST cru, sem passar pelo parser -- para achar bugs de
     * layout.c em formas de árvore que o parser atual não produz (mas
     * que podem aparecer se o parser mudar), e para isolar layout de
     * parser ao investigar um bug específico.
     * ================================================================== */

    run_ast("AST_SUM com left/right diretos",
        mk(AST_SUM, mk_text("0"), mk_text("n")));

    run_ast("AST_SUM genérico via SUPERSCRIPT(SUBSCRIPT(sum,0),n)",
        mk(AST_SUPERSCRIPT,
            mk(AST_SUBSCRIPT, mk(AST_SUM, NULL, NULL), mk_text("0")),
            mk_text("n")));

    {
        Ast *children[] = {
            mk(AST_SUM, mk_text("0"), mk_text("n")),
            mk_text("=x")
        };
        run_ast("sum seguido de \"=x\" numa sequência crua (checa baseline)",
            mk_seq(children, 2));
    }

    run_ast("superscript cuja base é multi-linha (fração)",
        mk(AST_SUPERSCRIPT,
            mk(AST_FRACTION, mk_text("a"), mk_text("b")),
            mk_text("2")));

    run_ast("sub E superscript, ambos multi-linha",
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
        run_ast("sequência terminando num operador grande sem termo colado",
            mk_seq(children, 2));
    }

    return 0;
}
