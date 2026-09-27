#include "parser.h"
#include "lexer.h"
#include "ast.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Parser types                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    Lexer lexer;
    Token current;
    int current_had_space;
    int depth;
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

/* ------------------------------------------------------------------ */
/* Parser helpers                                                      */
/* ------------------------------------------------------------------ */

static void advance(Parser *parser)
{
    unsigned char next_char =
        (unsigned char)parser->lexer.input[parser->lexer.position];

    parser->current_had_space = isspace(next_char) != 0;
    parser->current = lexer_next(&parser->lexer);
}

static int starts_term(TokenType type)
{
    switch (type) {
    case TOKEN_TEXT:
    case TOKEN_COMMAND:
    case TOKEN_LBRACE:
    case TOKEN_LPAREN:
        return 1;

    default:
        return 0;
    }
}

static int is_bare_operator(const Ast *node)
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

static int is_big_operator(AstType type)
{
    return type == AST_SUM ||
           type == AST_INT ||
           type == AST_PROD;
}

static Ast *parse_expression(Parser *parser);
static Ast *parse_atom(Parser *parser);

/* ------------------------------------------------------------------ */
/* Mathematical styles                                                 */
/* ------------------------------------------------------------------ */

static char *math_style(
    const char *text,
    const char *(*style_letter)(char)
)
{
    size_t capacity = strlen(text) * 4 + 1;
    char *output = malloc(capacity);

    if (!output)
        return NULL;

    size_t offset = 0;

    for (const char *p = text; *p; p++) {
        const char *replacement = style_letter(*p);

        if (replacement) {
            size_t length = strlen(replacement);

            memcpy(output + offset, replacement, length);
            offset += length;
        } else {
            output[offset++] = *p;
        }
    }

    output[offset] = '\0';

    return output;
}

static const char *mathbb_letter(char c)
{
    switch (c) {
    case 'A': return "\U0001D538";
    case 'B': return "\U0001D539";
    case 'C': return "\u2102";
    case 'D': return "\U0001D53B";
    case 'E': return "\U0001D53C";
    case 'F': return "\U0001D53D";
    case 'G': return "\U0001D53E";
    case 'H': return "\u210D";
    case 'I': return "\U0001D540";
    case 'J': return "\U0001D541";
    case 'K': return "\U0001D542";
    case 'L': return "\U0001D543";
    case 'M': return "\U0001D544";
    case 'N': return "\u2115";
    case 'O': return "\U0001D546";
    case 'P': return "\u2119";
    case 'Q': return "\u211A";
    case 'R': return "\u211D";
    case 'S': return "\U0001D54A";
    case 'T': return "\U0001D54B";
    case 'U': return "\U0001D54C";
    case 'V': return "\U0001D54D";
    case 'W': return "\U0001D54E";
    case 'X': return "\U0001D54F";
    case 'Y': return "\U0001D550";
    case 'Z': return "\u2124";

    case 'a': return "\U0001D552";
    case 'b': return "\U0001D553";
    case 'c': return "\U0001D554";
    case 'd': return "\U0001D555";
    case 'e': return "\U0001D556";
    case 'f': return "\U0001D557";
    case 'g': return "\U0001D558";
    case 'h': return "\U0001D559";
    case 'i': return "\U0001D55A";
    case 'j': return "\U0001D55B";
    case 'k': return "\U0001D55C";
    case 'l': return "\U0001D55D";
    case 'm': return "\U0001D55E";
    case 'n': return "\U0001D55F";
    case 'o': return "\U0001D560";
    case 'p': return "\U0001D561";
    case 'q': return "\U0001D562";
    case 'r': return "\U0001D563";
    case 's': return "\U0001D564";
    case 't': return "\U0001D565";
    case 'u': return "\U0001D566";
    case 'v': return "\U0001D567";
    case 'w': return "\U0001D568";
    case 'x': return "\U0001D569";
    case 'y': return "\U0001D56A";
    case 'z': return "\U0001D56B";

    default:
        return NULL;
    }
}

static const char *mathcal_letter(char c)
{
    switch (c) {
    case 'A': return "\U0001D49C";
    case 'B': return "\u212C";
    case 'C': return "\U0001D49E";
    case 'D': return "\U0001D49F";
    case 'E': return "\u2130";
    case 'F': return "\u2131";
    case 'G': return "\U0001D4A2";
    case 'H': return "\u210B";
    case 'I': return "\u2110";
    case 'J': return "\U0001D4A5";
    case 'K': return "\U0001D4A6";
    case 'L': return "\u2112";
    case 'M': return "\u2133";
    case 'N': return "\U0001D4A9";
    case 'O': return "\U0001D4AA";
    case 'P': return "\U0001D4AB";
    case 'Q': return "\U0001D4AC";
    case 'R': return "\u211B";
    case 'S': return "\U0001D4AE";
    case 'T': return "\U0001D4AF";
    case 'U': return "\U0001D4B0";
    case 'V': return "\U0001D4B1";
    case 'W': return "\U0001D4B2";
    case 'X': return "\U0001D4B3";
    case 'Y': return "\U0001D4B4";
    case 'Z': return "\U0001D4B5";

    default:
        return NULL;
    }
}

static const char *math_italic_letter(char c)
{
    switch (c) {
    case 'A': return "\U0001D434";
    case 'B': return "\U0001D435";
    case 'C': return "\U0001D436";
    case 'D': return "\U0001D437";
    case 'E': return "\U0001D438";
    case 'F': return "\U0001D439";
    case 'G': return "\U0001D43A";
    case 'H': return "\U0001D43B";
    case 'I': return "\U0001D43C";
    case 'J': return "\U0001D43D";
    case 'K': return "\U0001D43E";
    case 'L': return "\U0001D43F";
    case 'M': return "\U0001D440";
    case 'N': return "\U0001D441";
    case 'O': return "\U0001D442";
    case 'P': return "\U0001D443";
    case 'Q': return "\U0001D444";
    case 'R': return "\U0001D445";
    case 'S': return "\U0001D446";
    case 'T': return "\U0001D447";
    case 'U': return "\U0001D448";
    case 'V': return "\U0001D449";
    case 'W': return "\U0001D44A";
    case 'X': return "\U0001D44B";
    case 'Y': return "\U0001D44C";
    case 'Z': return "\U0001D44D";

    case 'a': return "\U0001D44E";
    case 'b': return "\U0001D44F";
    case 'c': return "\U0001D450";
    case 'd': return "\U0001D451";
    case 'e': return "\U0001D452";
    case 'f': return "\U0001D453";
    case 'g': return "\U0001D454";
    case 'h': return "\u210E";
    case 'i': return "\U0001D456";
    case 'j': return "\U0001D457";
    case 'k': return "\U0001D458";
    case 'l': return "\U0001D459";
    case 'm': return "\U0001D45A";
    case 'n': return "\U0001D45B";
    case 'o': return "\U0001D45C";
    case 'p': return "\U0001D45D";
    case 'q': return "\U0001D45E";
    case 'r': return "\U0001D45F";
    case 's': return "\U0001D460";
    case 't': return "\U0001D461";
    case 'u': return "\U0001D462";
    case 'v': return "\U0001D463";
    case 'w': return "\U0001D464";
    case 'x': return "\U0001D465";
    case 'y': return "\U0001D466";
    case 'z': return "\U0001D467";

    default:
        return NULL;
    }
}

static char *math_italicize(const char *text)
{
    size_t capacity = strlen(text) * 4 + 1;
    char *output = malloc(capacity);

    if (!output)
        return NULL;

    size_t offset = 0;

    for (const char *p = text; *p; p++) {
        const char *replacement = math_italic_letter(*p);

        if (replacement) {
            size_t length = strlen(replacement);

            memcpy(output + offset, replacement, length);
            offset += length;
        } else {
            output[offset++] = *p;
        }
    }

    output[offset] = '\0';

    return output;
}

/* ------------------------------------------------------------------ */
/* Styled commands                                                     */
/* ------------------------------------------------------------------ */

static Ast *parse_styled(
    Parser *parser,
    const char *(*style_letter)(char)
)
{
    int braced = parser->current.type == TOKEN_LBRACE;

    if (braced)
        advance(parser);

    if (parser->current.type != TOKEN_TEXT) {
        if (braced && parser->current.type == TOKEN_RBRACE)
            advance(parser);

        return ast_text("");
    }

    char *styled = math_style(
        parser->current.value,
        style_letter
    );

    Ast *node = ast_text(
        styled ? styled : parser->current.value
    );

    free(styled);
    advance(parser);

    if (braced && parser->current.type == TOKEN_RBRACE)
        advance(parser);

    return node;
}

static Ast *parse_mathbb(Parser *parser)
{
    advance(parser);
    return parse_styled(parser, mathbb_letter);
}

static Ast *parse_mathcal(Parser *parser)
{
    advance(parser);
    return parse_styled(parser, mathcal_letter);
}

/* ------------------------------------------------------------------ */
/* Atom parsers                                                        */
/* ------------------------------------------------------------------ */

static Ast *parse_text(Parser *parser)
{
    char *italic = math_italicize(parser->current.value);

    Ast *node = ast_text(
        italic ? italic : parser->current.value
    );

    free(italic);
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

    if (!node)
        return NULL;

    ast_add(node, ast_text("("));

    advance(parser);

    Ast *inside = parse_expression(parser);

    if (inside)
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

    if (!numerator)
        return ast_text("\\frac");

    Ast *denominator = parse_atom(parser);

    if (!denominator) {
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

    if (!top)
        return ast_text("\\binom");

    Ast *bottom = parse_atom(parser);

    if (!bottom) {
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

    if (!radicand)
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

static Ast *parse_prod(Parser *parser)
{
    advance(parser);
    return ast_new(AST_PROD);
}

static Ast *parse_lim(Parser *parser)
{
    advance(parser);
    return ast_new(AST_LIM);
}

static Ast *parse_symbol(Parser *parser, const char *value)
{
    Ast *node = ast_text(value);
    advance(parser);

    return node;
}

/* ------------------------------------------------------------------ */
/* Command table                                                       */
/* ------------------------------------------------------------------ */

static const CommandRule command_rules[] = {
    {"frac",           parse_frac,    NULL},
    {"binom",          parse_binom,   NULL},
    {"sqrt",           parse_sqrt,    NULL},
    {"sum",            parse_sum,     NULL},
    {"int",            parse_int,     NULL},
    {"prod",           parse_prod,    NULL},
    {"lim",            parse_lim,     NULL},
    {"mathbb",         parse_mathbb,  NULL},
    {"mathcal",        parse_mathcal, NULL},

    {"alpha",          NULL, "α"},
    {"beta",           NULL, "β"},
    {"gamma",          NULL, "γ"},
    {"delta",          NULL, "δ"},
    {"epsilon",        NULL, "ε"},
    {"zeta",           NULL, "ζ"},
    {"eta",            NULL, "η"},
    {"theta",          NULL, "θ"},
    {"lambda",         NULL, "λ"},
    {"mu",             NULL, "μ"},
    {"nu",             NULL, "ν"},
    {"xi",             NULL, "ξ"},
    {"pi",             NULL, "π"},
    {"rho",            NULL, "ρ"},
    {"sigma",          NULL, "σ"},
    {"tau",            NULL, "τ"},
    {"phi",            NULL, "φ"},
    {"chi",            NULL, "χ"},
    {"psi",            NULL, "ψ"},
    {"omega",          NULL, "ω"},

    {"Gamma",          NULL, "Γ"},
    {"Delta",          NULL, "Δ"},
    {"Theta",          NULL, "Θ"},
    {"Lambda",         NULL, "Λ"},
    {"Xi",             NULL, "Ξ"},
    {"Pi",             NULL, "Π"},
    {"Sigma",          NULL, "Σ"},
    {"Phi",            NULL, "Φ"},
    {"Psi",            NULL, "Ψ"},
    {"Omega",          NULL, "Ω"},

    {"infty",          NULL, "∞"},
    {"leq",            NULL, "≤"},
    {"geq",            NULL, "≥"},
    {"neq",            NULL, "≠"},
    {"times",          NULL, "×"},
    {"cdot",           NULL, "·"},
    {"pm",             NULL, "±"},
    {"mp",             NULL, "∓"},
    {"div",            NULL, "÷"},
    {"approx",         NULL, "≈"},
    {"equiv",          NULL, "≡"},
    {"propto",         NULL, "∝"},
    {"sim",            NULL, "∼"},

    {"in",             NULL, "∈"},
    {"notin",          NULL, "∉"},
    {"subset",         NULL, "⊂"},
    {"subseteq",       NULL, "⊆"},
    {"cup",            NULL, "∪"},
    {"cap",            NULL, "∩"},
    {"emptyset",       NULL, "∅"},
    {"forall",         NULL, "∀"},
    {"exists",         NULL, "∃"},
    {"lor",            NULL, "∨"},
    {"land",           NULL, "∧"},
    {"neg",            NULL, "¬"},

    {"rightarrow",     NULL, "→"},
    {"leftarrow",      NULL, "←"},
    {"uparrow",        NULL, "↑"},
    {"downarrow",      NULL, "↓"},
    {"leftrightarrow", NULL, "↔"},
    {"Rightarrow",     NULL, "⇒"},
    {"Leftarrow",      NULL, "⇐"},
    {"Leftrightarrow", NULL, "⇔"},

    {"partial",        NULL, "∂"},
    {"nabla",          NULL, "∇"},
    {"circ",           NULL, "∘"},
    {"ohime",          NULL, "♥"},
    {"ast",            NULL, "∗"},
    {"angle",          NULL, "∠"},
    {"hbar",           NULL, "ℏ"},

    {"le",             NULL, "≤"},
    {"ge",             NULL, "≥"},
    {"to",             NULL, "→"},
    {"mid",            NULL, "|"},
    {"dots",           NULL, "…"},
    {"ldots",          NULL, "…"},
    {"cdots",          NULL, "⋯"},
};

static Ast *parse_command(Parser *parser)
{
    const char *name = parser->current.value;
    size_t count = sizeof(command_rules) /
                   sizeof(command_rules[0]);

    for (size_t i = 0; i < count; i++) {
        const CommandRule *rule = &command_rules[i];

        if (strcmp(rule->name, name) != 0)
            continue;

        if (rule->value)
            return parse_symbol(parser, rule->value);

        return rule->parser(parser);
    }

    return parse_symbol(parser, name);
}

/* ------------------------------------------------------------------ */
/* Atom dispatch                                                       */
/* ------------------------------------------------------------------ */

static const AtomRule atom_rules[] = {
    {TOKEN_TEXT,     parse_text},
    {TOKEN_OPERATOR, parse_operator},
    {TOKEN_LBRACE,   parse_group},
    {TOKEN_LPAREN,   parse_parentheses},
    {TOKEN_COMMAND,  parse_command},
};

static Ast *parse_atom(Parser *parser)
{
    if (parser->depth >= MAX_PARSE_DEPTH)
        return NULL;

    parser->depth++;

    Ast *node = NULL;
    size_t count = sizeof(atom_rules) /
                   sizeof(atom_rules[0]);

    for (size_t i = 0; i < count; i++) {
        if (atom_rules[i].type == parser->current.type) {
            node = atom_rules[i].parser(parser);
            break;
        }
    }

    parser->depth--;

    return node;
}

/* ------------------------------------------------------------------ */
/* Term and expression parsing                                         */
/* ------------------------------------------------------------------ */

static Ast *parse_term(Parser *parser)
{
    Ast *node = parse_atom(parser);

    if (!node)
        return NULL;

    while (parser->current.type == TOKEN_CARET ||
           parser->current.type == TOKEN_UNDERSCORE) {

        if (parser->current.type == TOKEN_CARET) {
            advance(parser);

            Ast *exponent = parse_atom(parser);

            if (exponent) {
                if (is_big_operator(node->type) ||
                    node->type == AST_LIM) {
                    ast_free(node->right);
                    node->right = exponent;
                } else {
                    node = ast_binary(
                        AST_SUPERSCRIPT,
                        node,
                        exponent
                    );
                }
            }
        } else {
            advance(parser);

            Ast *subscript = parse_atom(parser);

            if (subscript) {
                if (is_big_operator(node->type) ||
                    node->type == AST_LIM) {
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

    return node;
}

static Ast *parse_expression(Parser *parser)
{
    Ast *sequence = ast_sequence();

    if (!sequence)
        return NULL;

    while (parser->current.type != TOKEN_EOF &&
           parser->current.type != TOKEN_RBRACE &&
           parser->current.type != TOKEN_RPAREN) {

        int had_space = parser->current_had_space;
        Ast *node = parse_term(parser);

        if (!node) {
            advance(parser);
            continue;
        }

        if (is_big_operator(node->type) &&
            starts_term(parser->current.type)) {

            Ast *term = parse_term(parser);

            if (term) {
                Ast *block = ast_sequence();

                if (!block) {
                    ast_free(term);
                    ast_free(node);
                    ast_free(sequence);
                    return NULL;
                }

                ast_add(block, node);
                ast_add(block, term);

                node = block;
            }
        }

        if (had_space && sequence->child_count > 0) {
            Ast *previous =
                sequence->children[sequence->child_count - 1];

            if (!is_bare_operator(node) &&
                !is_bare_operator(previous))
                ast_add(sequence, ast_text(" "));
        }

        ast_add(sequence, node);
    }

    return sequence;
}

/* ------------------------------------------------------------------ */
/* Public parser interface                                             */
/* ------------------------------------------------------------------ */

Ast *parse(const char *input)
{
    Parser parser;

    lexer_init(&parser.lexer, input);

    parser.current_had_space = 0;
    parser.depth = 0;
    parser.current = lexer_next(&parser.lexer);

    Ast *root = parse_expression(&parser);

    token_free(&parser.current);

    return root;
}
