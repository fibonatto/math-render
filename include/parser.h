#ifndef MATH_RENDER_PARSER_H
#define MATH_RENDER_PARSER_H
#define MAX_PARSE_DEPTH 500

#include "ast.h"

Ast *parse(const char *input);

#endif
