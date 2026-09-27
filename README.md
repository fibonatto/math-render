# math-render

A small terminal-based mathematical expression renderer written in C.

`math-render` parses a lightweight LaTeX-like syntax into an AST, computes a terminal layout, and renders the resulting expression using Unicode characters.

The goal is not to reproduce TeX typography. Instead, `math-render` provides a compact way to render mathematical notation directly in a terminal.

## Example

```text
./math-render '\sum_0^n \frac{a_k}{k!} + \int_0^x \frac{t^2}{1+t^2}'
```

Output:

```text
                         𝑛        𝑥
                        ───  𝑎    ⌠
                        ╲     𝑘   |    𝑡²
                        ╱⎽⎽  —— + |  ——————
                         0   𝑘!   ⌡  1 + 𝑡²
                                  0
```

Expressions can also be read from a file:

```text
./math-render -f formula.tex
```

Or from standard input:

```text
printf '%s\n' '\frac{a}{b}' | ./math-render
```

## Features

### Basic expressions

Single Latin letters are rendered as mathematical italic characters:

```text
x
```

```text
𝑥
```

Multiple letters are rendered individually:

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

```text
123
```

Mixed expressions are supported:

```text
x1y2
```

```text
𝑥1𝑦2
```

### Binary operators

The following operators are supported:

```text
+  -  =  *  /  <  >
```

For example:

```text
a+b-c=d
```

renders as:

```text
𝑎 + 𝑏 - 𝑐 = 𝑑
```

Unary minus is currently rendered as a normal operator:

```text
-x
```

```text
- 𝑥
```

### Superscripts and subscripts

Simple superscripts:

```text
x^2
```

```text
𝑥²
```

Subscripts:

```text
x_1
```

```text
𝑥₁
```

Both orders are supported:

```text
x_1^2
```

and:

```text
x^2_1
```

Grouped expressions can be used for multi-character scripts:

```text
x^{10}
```

Nested scripts are also supported:

```text
x^{y^z}
```

Repeated superscripts are parsed:

```text
x^2^3
```

### Fractions

Basic fractions:

```text
\frac{a}{b}
```

```text
𝑎
—
𝑏
```

Fractions can be nested:

```text
\frac{a}{\frac{b}{c}}
```

and can contain scripted expressions:

```text
\frac{a^2}{b}
```

Fractions can also contain larger expressions such as sums:

```text
\frac{\sum_0^n a_k}{n}
```

### Binomial coefficients

```text
\binom{n}{k}
```

renders using terminal-friendly delimiters:

```text
/𝑛\
\𝑘/
```

### Square roots

Basic square roots:

```text
\sqrt{x}
```

Nested roots are supported:

```text
\sqrt{\sqrt{x}}
```

Roots can contain compound expressions:

```text
\sqrt{\frac{a}{b}}
```

### Large operators

Summation:

```text
\sum_0^n a_k
```

The lower and upper limits are rendered around a multi-line summation symbol.

Both limits are optional:

```text
\sum
```

```text
\sum_0
```

```text
\sum^n
```

The summation operator can be followed directly by its term:

```text
\sum_0^n \frac{a_k}{k!}
```

It also works as part of a larger expression:

```text
\sum_0^n a_k + x
```

Integral:

```text
\int_0^x f(x)
```

and without limits:

```text
\int
```

Large operators are laid out as multi-line terminal structures rather than relying on a single Unicode glyph.

### Parentheses and groups

Parentheses are supported:

```text
(a+b)
```

Nested parentheses work as well:

```text
((a+b)*c)
```

Curly braces are used as grouping constructs and are not rendered themselves:

```text
{a+b}
```

Unclosed delimiters are handled without crashing:

```text
(a+b
```

and:

```text
{a+b
```

### Greek letters and mathematical symbols

Common Greek letters are supported:

```text
\alpha + \beta = \gamma
```

```text
α + β = γ
```

Uppercase Greek letters are also supported:

```text
\Gamma \Delta \Omega
```

Other supported mathematical symbols include:

```text
\infty
\leq
\geq
\neq
\times
\cdot
\pm
```

Set and logic symbols include:

```text
\in
\notin
\subset
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

## Unknown commands

Unknown commands currently fall back to their command name instead of causing a parse failure.

For example:

```text
\foo
```

renders as:

```text
foo
```

This also means that commands not currently implemented as mathematical operators are treated as text.

For example, `\prod` and `\lim` are currently not implemented as dedicated large operators.

```text
\prod_0^n a_k
```

and:

```text
\lim_{x \to 0} f(x)
```

therefore do not receive specialized operator layouts.

## Input modes

### Direct expression

Pass a single expression as the argument:

```text
./math-render '\frac{a}{b}'
```

Using single quotes is recommended so the shell does not interpret backslashes or other special characters.

### File input

Put the expression in a file:

```text
formula.tex
```

```text
\sum_0^n \frac{a_k}{k!} + \int_0^x \frac{t^2}{1+t^2}
```

Then:

```text
./math-render -f formula.tex
```

### Standard input

With no arguments, `math-render` reads from standard input:

```text
printf '%s\n' '\frac{a}{b}' | ./math-render
```

This also allows it to be used in shell pipelines.

## Architecture

The renderer is organized into several stages:

```text
Input
  │
  ▼
Parser
  │
  ▼
AST
  │
  ▼
Layout
  │
  ▼
Box tree
  │
  ▼
Renderer
  │
  ▼
Terminal
```

The main components are:

| Component    | Responsibility                                       |
| ------------ | ---------------------------------------------------- |
| `parser.c`   | Parses the input expression                          |
| `ast.c`      | Defines and manages the abstract syntax tree         |
| `layout.c`   | Converts the AST into terminal-oriented layout boxes |
| `renderer.c` | Renders the final layout                             |
| `main.c`     | Handles input and coordinates the pipeline           |

The separation between parsing, layout, and rendering allows multi-line mathematical structures such as fractions, roots, sums, and integrals to be composed recursively.

## Design

`math-render` treats mathematical expressions as structured layouts rather than strings.

For example:

```text
\frac{a^2+b^2}{c}
```

is represented structurally before being converted into terminal rows.

This makes it possible to compose constructs such as:

```text
\sqrt{\frac{a^2+b^2}{c}}
```

or:

```text
\frac{\sum_0^n a_k}{n}
```

without requiring each combination to have a dedicated rendering rule.

The layout system also tracks dimensions and baselines so that multi-line expressions can be combined with surrounding expressions.

## Current limitations

This is an early-stage renderer, not a full LaTeX implementation.

Notable limitations include:

* Only a subset of LaTeX-like commands is implemented.
* Unknown commands fall back to textual output.
* Unary minus does not yet have specialized mathematical spacing.
* User whitespace is not preserved as literal terminal spacing in all cases.
* Some mathematical operators such as `\prod` and `\lim` are not implemented as large operators.
* Terminal rendering depends on Unicode glyphs and the terminal's font.
* Unicode width and glyph appearance can vary between terminal environments.
* The renderer is designed around terminal output rather than TeX-compatible typography.

## Testing

The project includes tests covering:

* basic identifiers
* digits
* operators
* superscripts
* subscripts
* nested scripts
* fractions
* nested fractions
* binomial coefficients
* square roots
* nested roots
* summations
* integrals
* nested mathematical structures
* grouping
* parentheses
* Greek letters
* mathematical relations
* set and logic symbols
* arrows
* fallback behavior
* multi-line baseline alignment
* wide expressions
* large operators embedded inside other expressions

Example expressions include the quadratic formula:

```text
x = \frac{-b \pm \sqrt{b^2-4ac}}{2a}
```

and the binomial theorem:

```text
\binom{n}{k} = \frac{n!}{k!(n-k)!}
```

A larger layout test:

```text
\sum_0^n \frac{a_k}{k!}
+ \int_0^x \frac{t^2}{1+t^2}
+ \sqrt{\frac{a^2+b^2}{c}}
= \infty
```

## Requirements

A C compiler and a terminal with Unicode support are required.

The output uses Unicode mathematical characters and box-drawing or mathematical glyphs, so the selected terminal font must contain the required characters.

## Status

`math-render` is experimental software.

The parser, AST, layout engine, and renderer are being developed together, with particular attention to recursive layout, baseline alignment, multi-line expressions, and robust terminal rendering.

