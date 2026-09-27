# math-render

A small terminal-based mathematical expression renderer written in C11.

`math-render` parses a lightweight LaTeX-like mathematical syntax into an abstract syntax tree, transforms the AST into a two-dimensional terminal layout, and renders the result using Unicode characters.

The project is designed around structured layout rather than string substitution. Fractions, roots, scripts, large operators, and nested expressions are represented as composable layout objects with explicit dimensions and baselines.

It is not intended to be a TeX implementation. The goal is to provide a compact and predictable way to render mathematical notation directly in a terminal.

## Example

```sh
./math-render '\sum_0^n \frac{a_k}{k!} + \int_0^x \frac{t^2}{1+t^2}'
```

Output:

```text
                               𝑛        𝑥
                              ───       ⌠
                              ╲    𝑎ₖ   |    𝑡²
                              ╱⎽⎽  —— + |  ——————
                               0   𝑘!   ⌡  1 + 𝑡²
                                        0
```

Expressions can also be read from files:

```sh
./math-render -f formula.tex
```

or from standard input:

```sh
printf '%s\n' '\frac{a}{b}' | ./math-render
```

## Features

### Mathematical identifiers

Latin letters are converted to mathematical italic Unicode characters when an equivalent glyph is available.

```text
x
```

```text
𝑥
```

Multiple letters are handled independently:

```text
xy
```

```text
𝑥𝑦
```

Digits remain upright:

```text
123
```

Mixed identifiers preserve this distinction:

```text
x1y2
```

```text
𝑥1𝑦2
```

The same conversion is applied inside larger expressions.

### Operators

Basic binary operators are supported:

```text
+  -  =  *  /  <  >
```

Whitespace around operators is normalized:

```text
a+b
```

and:

```text
a + b
```

produce the same structural spacing:

```text
𝑎 + 𝑏
```

Operator sequences can be composed:

```text
a+b-c=d
```

```text
𝑎 + 𝑏 - 𝑐 = 𝑑
```

Unary minus is currently represented using the regular subtraction operator:

```text
-x
```

```text
- 𝑥
```

This is a known limitation of the current parser/layout model.

### Superscripts and subscripts

Single-character scripts are rendered inline when Unicode provides an appropriate subscript or superscript character.

```text
x_1
```

```text
𝑥₁
```

```text
x^2
```

```text
𝑥²
```

Alphabetic scripts are also supported when a Unicode equivalent exists:

```text
a_k
```

```text
𝑎ₖ
```

Both script orders are supported:

```text
x_1^2
```

```text
𝑥₁²
```

and:

```text
x^2_1
```

```text
𝑥²₁
```

Grouped scripts allow multi-character expressions:

```text
x^{10}
```

```text
  10
  𝑥
```

Scripts are recursively composable:

```text
x^{y^z}
```

```text
  𝑦ᶻ
  𝑥
```

Repeated superscripts are parsed as nested script operations:

```text
x^2^3
```

```text
𝑥²³
```

When no dedicated Unicode character exists, the layout engine falls back to a multi-row representation.

For example, an uppercase subscript such as:

```text
A_T
```

is represented structurally rather than forced into an invalid Unicode conversion.

### Fractions

Fractions are represented as independent numerator and denominator boxes separated by a horizontal rule.

```text
\frac{a}{b}
```

```text
  𝑎
  —
  𝑏
```

Fractions are recursive and can contain arbitrary expressions:

```text
\frac{a^2}{b}
```

```text
  𝑎²
  ——
   𝑏
```

Nested fractions work naturally:

```text
\frac{a}{\frac{b}{c}}
```

The same mechanism allows larger constructs to be embedded inside fractions:

```text
\frac{\sum_0^n a_k}{n}
```

### Binomial coefficients

Binomial coefficients are rendered using terminal-friendly delimiters:

```text
\binom{n}{k}
```

```text
/𝑛\
\𝑘/
```

The construction is layout-based rather than dependent on a single Unicode glyph.

### Square roots

Square roots use a constructed radical layout.

```text
\sqrt{x}
```

```text
 ─
√𝑥
```

Nested roots are supported:

```text
\sqrt{\sqrt{x}}
```

Roots can contain arbitrary expressions:

```text
\sqrt{\frac{a}{b}}
```

```text
  ─
 /𝑎
 │—
 √𝑏
```

### Large operators

The renderer provides dedicated layouts for operators that naturally occupy multiple terminal rows.

#### Summation

```text
\sum_0^n a_k
```

```text
  𝑛
 ───
 ╲
 ╱⎽⎽  𝑎ₖ
  0
```

Upper and lower limits are optional:

```text
\sum
```

```text
\sum_0
```

```text
\sum^n
```

Large operators can be followed by an attached expression:

```text
\sum_0^n \frac{a_k}{k!}
```

and can appear inside other structures such as fractions:

```text
\frac{\sum_0^n a_k}{n}
```

#### Product

Products use a dedicated multi-row layout:

```text
\prod_0^n a_k
```

```text
  𝑛
 ───
 │ │
 │ │  𝑎ₖ
  0
```

Compound limits are supported:

```text
\prod_{i=1}^n i
```

```text
  𝑛
 ───
 │ │   𝑖
 │ │
 𝑖 = 1
```

#### Integral

Integrals are constructed vertically rather than rendered using a single fixed glyph:

```text
\int_0^x f(x)
```

```text
  𝑥
  ⌠
  |
  |  𝑓(𝑥)
  ⌡
  0
```

The integral sign itself is extended to match the height required by its surrounding layout.

#### Limit

Limits are represented as an operator with an optional condition:

```text
\lim_{x \to 0} f(x)
```

```text
lim  𝑓(𝑥)
𝑥 → 0
```

A bare limit is also supported:

```text
\lim f(x)
```

The limit condition is laid out relative to the operator baseline instead of being treated as an ordinary subscript.

### Greek letters and mathematical symbols

Common Greek letters are supported through LaTeX-like commands:

```text
\alpha
\beta
\gamma
\Gamma
\Delta
\Omega
```

For example:

```text
\alpha + \beta = \gamma
```

```text
α + β = γ
```

Supported relations and operators include:

```text
\infty
\leq
\geq
\neq
\times
\cdot
\pm
\le
\ge
\to
```

Set theory and logic symbols include:

```text
\in
\notin
\subset
\subseteq
\cup
\cap
\forall
\exists
```

Arrows include:

```text
\rightarrow
\Rightarrow
\leftrightarrow
```

Calculus-related symbols include:

```text
\partial
\nabla
\hbar
```

Dots are supported as well:

```text
\dots
\ldots
\cdots
```

### Delimiters and grouping

Parentheses are represented as ordinary layout characters and can contain arbitrary expressions:

```text
(a+b)
```

Nested parentheses are supported:

```text
((a+b)*c)
```

Curly braces are grouping constructs and are not emitted as visible delimiters:

```text
{a+b}
```

Groups can contain whitespace and nested structures.

The parser also handles incomplete input without crashing. For example:

```text
(a+b
```

and:

```text
{a+b
```

are rendered using the expression parsed before the missing closing delimiter.

Unmatched closing delimiters are ignored according to the parser's recovery rules.

### Mathematical fonts

The renderer supports selected Unicode mathematical alphabets.

Blackboard bold:

```text
\mathbb{P}
```

```text
ℙ
```

Single-atom arguments can also be written without braces:

```text
\mathbb R
```

```text
ℝ
```

Calligraphic characters are supported where Unicode provides the corresponding mathematical character:

```text
\mathcal{X}
```

```text
𝒳
```

Coverage is inherently limited by Unicode. For characters without a dedicated mathematical alphabet glyph, the renderer falls back to the available representation.

## Syntax

The input syntax intentionally resembles a small subset of LaTeX rather than attempting to implement the complete language.

Examples:

```text
x
x_1
x^2
x_1^2
x^{10}
\frac{a}{b}
\sqrt{x}
\sum_0^n a_k
\prod_{i=1}^n i
\int_0^x f(x)
\lim_{x\to\infty} \frac{1}{n}
\mathbb{P}
\mathcal{X}
```

Commands may consume braced groups or, for commands that accept a single atom, an unbraced atom.

For example:

```text
\mathbb{P}
```

and:

```text
\mathbb P
```

are both supported.

## Unknown commands and fallback behavior

Unknown commands do not cause an immediate parse error.

For example:

```text
\foo
```

falls back to:

```text
foo
```

This behavior allows unsupported commands to degrade into textual output instead of terminating the entire expression.

Malformed or incomplete constructs also use parser fallback/recovery behavior where possible.

Examples include:

```text
\frac{a}
\sqrt
x_
x^
x\
```

The renderer is therefore designed to remain usable with partially formed input, although the exact output for malformed expressions is implementation-defined.

## Input modes

### Command-line argument

A complete expression can be passed directly:

```sh
./math-render '\frac{a}{b}'
```

Single quotes are recommended because they prevent the shell from interpreting backslashes and other characters.

### File input

Expressions can be stored in a file:

```text
formula.tex
```

```text
\sum_0^n \frac{a_k}{k!}
```

and rendered with:

```sh
./math-render -f formula.tex
```

### Standard input

With no input file or expression argument, the program reads from standard input:

```sh
printf '%s\n' '\frac{a}{b}' | ./math-render
```

This makes `math-render` suitable for shell pipelines and other command-line workflows.

## Architecture

The implementation is divided into distinct stages:

```text
                  ┌──────────────┐
                  │    Input     │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │    Lexer     │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │    Parser    │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │     AST      │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │    Layout    │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │     Box      │
                  │    tree      │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │   Renderer   │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │   Terminal   │
                  └──────────────┘
```

### Lexer

`lexer.c` converts the input stream into tokens.

The lexer is responsible for recognizing syntax-level elements such as:

* identifiers
* digits
* operators
* commands
* braces
* parentheses
* commas
* script markers
* whitespace

The parser operates on these tokens rather than directly manipulating the input string.

### Parser

`parser.c` converts the token stream into an AST.

Parsing is recursive because expressions such as:

```text
\sqrt{\frac{a^2+b^2}{c}}
```

contain expressions nested inside other expressions.

The parser also handles command-specific argument rules, scripts, groups, operators, and parser recovery for incomplete input.

### AST

`ast.c` contains the representation and memory management for the abstract syntax tree.

The AST represents mathematical structure independently from terminal representation.

Conceptually, an expression such as:

```text
\frac{a^2}{b}
```

is represented as a fraction containing two child expressions rather than as a preformatted string.

This separation allows the same AST node to be rendered differently depending on its surrounding layout context.

### Layout

`layout.c` transforms AST nodes into terminal-oriented boxes.

A box contains:

* width
* height
* baseline information
* a two-dimensional array of terminal cells

Layout is recursive. A node first lays out its children and then combines their boxes according to the semantics of the construct.

Examples:

```text
fraction
├── numerator
├── horizontal rule
└── denominator
```

and:

```text
script
├── base
├── superscript
└── subscript
```

This allows complex expressions to be composed without requiring dedicated rendering code for every possible combination.

### Renderer

`renderer.c` converts the final box into terminal output.

The renderer is deliberately simple. Layout decisions are made before rendering, so the renderer does not need to understand mathematical semantics.

The output is centered according to the terminal width when the output stream is attached to a terminal. A fallback width is used when the output is redirected or piped.

## Layout model

The core design principle is that mathematical expressions are **two-dimensional objects**, not strings.

A normal text renderer can often concatenate strings:

```text
a + b
```

That model breaks down for expressions such as:

```text
  a
  —
  b
```

or:

```text
  n
 ───
 ╲
 ╱⎽⎽
  0
```

The layout engine therefore works with boxes that have explicit dimensions and a baseline.

When boxes are combined horizontally, their baselines are aligned.

When boxes are combined vertically, their dimensions are expanded to accommodate the required rows.

This model allows structures such as:

```text
\frac{\sum_0^n a_k}{n}
```

to be constructed recursively from existing layout primitives.

The same mechanism is used for nested fractions, roots, scripts, limits, products, sums, and integrals.

## Unicode handling

The renderer relies heavily on Unicode because terminal output has no native concept of mathematical italic, superscript, subscript, or extensible mathematical operators.

Where Unicode provides an appropriate mathematical character, the renderer uses it.

Examples include:

```text
𝑥
𝑎
𝑏
𝑥²
𝑥₁
ℙ
𝒳
α
∞
≤
→
```

For structures without a single suitable Unicode character, the renderer constructs the visual representation from multiple terminal characters.

Examples include:

* fractions
* summations
* products
* integrals
* roots
* binomial coefficients
* multi-row scripts

Unicode terminal rendering has unavoidable environment-dependent behavior. Glyph width, font coverage, and visual alignment may differ between terminals and fonts.

The layout engine therefore treats terminal cells as fixed columns while relying on the selected terminal font to provide compatible glyph metrics.

## Error handling and parser recovery

The parser is intentionally permissive.

The renderer should remain usable when an expression is incomplete, for example while an expression is being edited or generated incrementally.

Examples:

```text
x_
x^
\sqrt
\frac{a}
x\
(a+b
```

Where possible, incomplete constructs are reduced to the valid portion of the expression instead of causing a crash.

This behavior is part of the current parser design rather than an attempt to provide full LaTeX error diagnostics.

## Project structure

```text
math-render/
├── include/
│   ├── ast.h
│   ├── layout.h
│   ├── lexer.h
│   ├── parser.h
│   └── renderer.h
├── src/
│   ├── ast.c
│   ├── layout.c
│   ├── lexer.c
│   ├── main.c
│   ├── parser.c
│   └── renderer.c
├── test_main.c
├── Makefile
└── build.sh
```

Generated build artifacts are stored under:

```text
build/
```

The main executable is generated at:

```text
./math-render
```

The sanitizer-enabled test executable is generated at:

```text
./build/test_main
```

## Building

The project requires a C11-compatible compiler and standard POSIX terminal facilities.

Build the renderer with:

```sh
make
```

The default compilation flags are:

```text
-std=c11
-Wall
-Wextra
-Wpedantic
-O2
```

Include files are located through:

```text
-Iinclude
```

The resulting executable is:

```text
./math-render
```

A clean rebuild can be performed with:

```sh
make clean
make
```

The repository also provides:

```sh
./build.sh
```

which performs a clean build.

## Testing

The project includes a standalone test program covering both individual features and composed expressions.

Run the complete test suite with:

```sh
make test
```

Tests are compiled with:

```text
-std=c11
-Wall
-Wextra
-Wpedantic
-O2
-fsanitize=address,undefined
```

The test binary is:

```text
build/test_main
```

and is executed automatically by `make test`.

The test suite covers:

* identifiers and Unicode mathematical italic
* digits
* mixed identifiers
* whitespace handling
* operators
* chained operators
* unary minus behavior
* superscripts
* subscripts
* Unicode script conversion
* grouped scripts
* nested scripts
* fractions
* nested fractions
* fractions containing scripts
* fractions containing large operators
* binomial coefficients
* square roots
* nested roots
* roots containing fractions
* summation
* product
* integral
* limit
* optional large-operator limits
* compound limits
* parentheses
* grouping
* incomplete delimiters
* Greek letters
* relations
* set and logic symbols
* arrows
* calculus symbols
* ellipsis commands
* blackboard bold
* calligraphic characters
* unknown-command fallback
* empty input
* malformed or incomplete commands
* baseline alignment
* multi-row expressions
* nested multi-row expressions
* wide expressions
* terminal centering
* large operators embedded in larger expressions

The tests also include composed mathematical expressions such as the quadratic formula, the binomial theorem, and expressions combining products, limits, fractions, and probability notation.

## Installation

The executable can be installed into the user's local binary directory:

```sh
make install
```

This installs:

```text
~/.local/bin/math-render
```

To remove it:

```sh
make uninstall
```

## Current limitations

`math-render` implements a deliberately small subset of LaTeX-like mathematical syntax.

Current limitations include:

* It is not a LaTeX parser.
* Only explicitly implemented commands receive specialized semantics.
* Unknown commands fall back to textual output.
* Unary minus currently uses ordinary binary-operator spacing.
* User whitespace is normalized according to the parser's tokenization and layout rules.
* Unicode mathematical alphabets are limited by the characters defined by Unicode.
* Unicode glyph widths and visual metrics depend on the terminal and font.
* The renderer targets terminal cells rather than TeX's typography model.
* Delimiter sizing is not equivalent to TeX's dynamic delimiter system.
* Mathematical spacing is intentionally approximate.
* Error reporting is minimal because the parser favors recovery over diagnostics.
* The supported command set is intentionally much smaller than LaTeX.

These limitations are consequences of the project's scope rather than implementation goals that the renderer attempts to hide.

## Design goals

The project currently prioritizes:

1. **Structural representation**

   Mathematical expressions should be represented as trees rather than preformatted strings.

2. **Recursive composition**

   Existing layout primitives should compose naturally inside other primitives.

3. **Terminal-oriented rendering**

   Layout decisions should account for fixed terminal rows and columns.

4. **Explicit baseline management**

   Multi-line expressions should align correctly when combined with adjacent expressions.

5. **Unicode where appropriate**

   Unicode mathematical characters should be used when they provide a compact representation.

6. **Graceful handling of incomplete input**

   Partially formed expressions should not cause crashes.

7. **Small implementation surface**

   The project intentionally avoids reproducing the complexity of a complete TeX engine.

## Development

The project is implemented in C11 and uses no external runtime libraries for parsing or layout.

The primary dependencies are:

* a C11 compiler
* standard C library facilities
* POSIX terminal interfaces
* a Unicode-capable terminal/font

The codebase is divided so that parser changes do not require the renderer to understand syntax, and renderer changes do not require the parser to understand terminal layout.

This separation is particularly important for large mathematical structures, where the same AST node can be embedded in several different layout contexts.

## License

No license has been specified yet.

