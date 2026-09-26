#include "parser.h"
#include "lexer.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    Lexer lexer;
    Token current;
} Parser;

typedef Ast *(*AtomParser)(Parser *);
typedef Ast *(*CommandParser)(Parser *);

typedef struct {
    const char *name;
    CommandParser parser;
    const char *value;
} CommandRule;

typedef struct {
    TokenType type;
    AtomParser parser;
} AtomRule;

static void advance(Parser *parser)
{
    token_free(&parser->current);
    parser->current = lexer_next(&parser->lexer);
}

static Ast *parse_expression(Parser *parser);
static Ast *parse_atom(Parser *parser);

static Ast *parse_text(Parser *parser)
{
    Ast *node = ast_text(parser->current.value);
    advance(parser);

    return node;
}

static Ast *parse_operator(Parser *parser)
{
    Ast *node = ast_text(parser->current.value);
    advance(parser);

    return node;
}

static Ast *parse_group(Parser *parser)
{
    advance(parser);

    Ast *node = parse_expression(parser);

    if (parser->current.type == TOKEN_RBRACE)
        advance(parser);

    return node;
}

static Ast *parse_parentheses(Parser *parser)
{
    Ast *node = ast_sequence();

    ast_add(node, ast_text("("));

    advance(parser);

    Ast *inside = parse_expression(parser);

    if (inside != NULL)
        ast_add(node, inside);

    if (parser->current.type == TOKEN_RPAREN) {
        ast_add(node, ast_text(")"));
        advance(parser);
    }

    return node;
}

static Ast *parse_frac(Parser *parser)
{
    advance(parser);

    Ast *numerator = parse_atom(parser);

    if (numerator == NULL)
        return ast_text("\\frac");

    Ast *denominator = parse_atom(parser);

    if (denominator == NULL) {
        ast_free(numerator);
        return ast_text("\\frac");
    }

    return ast_binary(
        AST_FRACTION,
        numerator,
        denominator
    );
}

static Ast *parse_binom(Parser *parser)
{
    advance(parser);

    Ast *top = parse_atom(parser);

    if (top == NULL)
        return ast_text("\\binom");

    Ast *bottom = parse_atom(parser);

    if (bottom == NULL) {
        ast_free(top);
        return ast_text("\\binom");
    }

    return ast_binary(
        AST_BINOM,
        top,
        bottom
    );
}

static Ast *parse_sqrt(Parser *parser)
{
    advance(parser);

    Ast *radicand = parse_atom(parser);

    if (radicand == NULL)
        return ast_text("\\sqrt");

    return ast_unary(AST_SQRT, radicand);
}

static Ast *parse_sum(Parser *parser)
{
    advance(parser);

    return ast_new(AST_SUM);
}

static Ast *parse_int(Parser *parser)
{
    advance(parser);

    return ast_new(AST_INT);
}

static Ast *parse_symbol(Parser *parser, const char *value)
{
    Ast *node = ast_text(value);
    advance(parser);

    return node;
}

static const CommandRule command_rules[] = {
    // =========================================================
    // PARSED COMMANDS (Require logic to read arguments)
    // =========================================================
    {"frac",       parse_frac,  NULL},
    {"binom",      parse_binom, NULL},
    {"sum",        parse_sum,   NULL},
    {"int",        parse_int,   NULL},
    {"sqrt",       parse_sqrt,  NULL},
    // {"prod",       parse_prod,  NULL}, // Product (Π) - similar to sum
    // {"lim",        parse_lim,   NULL}, // Limits (place text below lim)
    
    // =========================================================
    // GREEK LETTERS
    // =========================================================
    // Lowercase
    {"alpha",      NULL, "α"},
    {"beta",       NULL, "β"},
    {"gamma",      NULL, "γ"},
    {"delta",      NULL, "δ"},
    {"epsilon",    NULL, "ε"},
    {"zeta",       NULL, "ζ"},
    {"eta",        NULL, "η"},
    {"theta",      NULL, "θ"},
    {"lambda",     NULL, "λ"},
    {"mu",         NULL, "μ"},
    {"nu",         NULL, "ν"},
    {"xi",         NULL, "ξ"},
    {"pi",         NULL, "π"},
    {"rho",        NULL, "ρ"},
    {"sigma",      NULL, "σ"},
    {"tau",        NULL, "τ"},
    {"phi",        NULL, "φ"},
    {"chi",        NULL, "χ"},
    {"psi",        NULL, "ψ"},
    {"omega",      NULL, "ω"},

    // Uppercase (Those distinct from the Latin alphabet)
    {"Gamma",      NULL, "Γ"},
    {"Delta",      NULL, "Δ"},
    {"Theta",      NULL, "Θ"},
    {"Lambda",     NULL, "Λ"},
    {"Xi",         NULL, "Ξ"},
    {"Pi",         NULL, "Π"},
    {"Sigma",      NULL, "Σ"},
    {"Phi",        NULL, "Φ"},
    {"Psi",        NULL, "Ψ"},
    {"Omega",      NULL, "Ω"},

    // =========================================================
    // MATHEMATICAL OPERATORS AND RELATIONS
    // =========================================================
    {"infty",      NULL, "∞"},
    {"leq",        NULL, "≤"},
    {"geq",        NULL, "≥"},
    {"neq",        NULL, "≠"},
    {"times",      NULL, "×"},
    {"cdot",       NULL, "·"},
    {"pm",         NULL, "±"}, // Plus-minus
    {"mp",         NULL, "∓"}, // Minus-plus
    {"div",        NULL, "÷"},
    {"approx",     NULL, "≈"}, // Approximately
    {"equiv",      NULL, "≡"}, // Equivalent
    {"propto",     NULL, "∝"}, // Proportional to
    {"sim",        NULL, "∼"}, // Similar to

    // =========================================================
    // SETS AND LOGIC
    // =========================================================
    {"in",         NULL, "∈"}, // Element of
    {"notin",      NULL, "∉"}, // Not an element of
    {"subset",     NULL, "⊂"}, // Subset
    {"subseteq",   NULL, "⊆"}, // Subset or equal to
    {"cup",        NULL, "∪"}, // Union
    {"cap",        NULL, "∩"}, // Intersection
    {"emptyset",   NULL, "∅"}, // Empty set
    {"forall",     NULL, "∀"}, // For all
    {"exists",     NULL, "∃"}, // Exists
    {"lor",        NULL, "∨"}, // Logical OR
    {"land",       NULL, "∧"}, // Logical AND
    {"neg",        NULL, "¬"}, // Logical NOT

    // =========================================================
    // ARROWS
    // =========================================================
    {"rightarrow", NULL, "→"},
    {"leftarrow",  NULL, "←"},
    {"uparrow",    NULL, "↑"},
    {"downarrow",  NULL, "↓"},
    {"leftrightarrow", NULL, "↔"},
    {"Rightarrow", NULL, "⇒"}, // Implies
    {"Leftarrow",  NULL, "⇐"},
    {"Leftrightarrow", NULL, "⇔"}, // If and only if

    // =========================================================
    // CALCULUS AND MISCELLANEOUS
    // =========================================================
    {"partial",    NULL, "∂"}, // Partial derivative
    {"nabla",      NULL, "∇"}, // Gradient / Nabla
    {"circ",       NULL, "∘"}, // Function composition
	{"ohime",	   NULL, "♥"}, // Ohime-chan
    {"ast",        NULL, "∗"}, // Asterisk operator
    {"angle",      NULL, "∠"}, // Angle
    {"hbar",       NULL, "ℏ"}, // Reduced Planck constant
};


static Ast *parse_command(Parser *parser)
{
    const char *name = parser->current.value;
    size_t count = sizeof(command_rules) / sizeof(command_rules[0]);

    for (size_t i = 0; i < count; i++) {
        const CommandRule *rule = &command_rules[i];

        if (strcmp(rule->name, name) != 0)
            continue;

        if (rule->value != NULL)
            return parse_symbol(parser, rule->value);

        return rule->parser(parser);
    }

    return parse_symbol(parser, name);
}

static const AtomRule atom_rules[] = {
    {TOKEN_TEXT,     parse_text},
    {TOKEN_OPERATOR, parse_operator},
    {TOKEN_LBRACE,   parse_group},
    {TOKEN_LPAREN,   parse_parentheses},
    {TOKEN_COMMAND,  parse_command},
};

static Ast *parse_atom(Parser *parser)
{
    size_t count = sizeof(atom_rules) / sizeof(atom_rules[0]);

    for (size_t i = 0; i < count; i++) {
        if (atom_rules[i].type == parser->current.type)
            return atom_rules[i].parser(parser);
    }

    return NULL;
}

static int is_big_operator(AstType type)
{
    /* \sum and \int don't have "sub"/"super" scripts in the usual
     * sense -- \sum_a^b attaches a lower and an upper *limit* to the
     * operator itself, and needs to render centered above/below it
     * (layout.c's stack_limits), not corner-offset like x_i or x^2.
     * So when the thing that just got parsed IS one of these bare
     * operators, a following _/^ fills its own left/right (lower/upper
     * limit) instead of wrapping it in a generic AST_SUBSCRIPT/
     * AST_SUPERSCRIPT node. Add AST_PROD (or similar) here too if it
     * gets implemented later. */
    return type == AST_SUM || type == AST_INT;
}

static Ast *parse_expression(Parser *parser)
{
    Ast *sequence = ast_sequence();

    while (
        parser->current.type != TOKEN_EOF &&
        parser->current.type != TOKEN_RBRACE &&
        parser->current.type != TOKEN_RPAREN
    ) {
        Ast *node = parse_atom(parser);

        if (!node) {
            advance(parser);
            continue;
        }

        while (parser->current.type == TOKEN_CARET ||
               parser->current.type == TOKEN_UNDERSCORE) {
            if (parser->current.type == TOKEN_CARET) {
                advance(parser);

                Ast *exponent = parse_atom(parser);

                if (exponent != NULL) {
                    if (is_big_operator(node->type)) {
                        ast_free(node->right); /* NULL first time; no-op */
                        node->right = exponent;
                    } else {
                        node = ast_binary(
                            AST_SUPERSCRIPT,
                            node,
                            exponent
                        );
                    }
                }
            } else if (parser->current.type == TOKEN_UNDERSCORE) {
                advance(parser);

                Ast *subscript = parse_atom(parser);

                if (subscript != NULL) {
                    if (is_big_operator(node->type)) {
                        ast_free(node->left);
                        node->left = subscript;
                    } else {
                        node = ast_binary(
                            AST_SUBSCRIPT,
                            node,
                            subscript
                        );
                    }
                }
            }
        }

        ast_add(sequence, node);
    }

    return sequence;
}

Ast *parse(const char *input)
{
    Parser parser;

    lexer_init(&parser.lexer, input);
    parser.current = lexer_next(&parser.lexer);

    Ast *root = parse_expression(&parser);

    token_free(&parser.current);

    return root;
}
