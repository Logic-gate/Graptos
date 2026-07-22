# Graptoς formatting options

The `formatting:` section is optional and belongs inside a language syntax
definition, such as `syntax/c.yaml`. If the section is missing, Graptoς leaves
that language unchanged when `Ctrl+J` is pressed.

Formatter structure still comes from the existing syntax fields:

- `auto_indent`
- `indent_openers`
- `indent_closers`
- `statement_required_enders`
- `line_comment`
- `rules`

The `formatting:` section only controls layout style. It should not duplicate
structural syntax fields.

Example:

```yaml
formatting:
  enabled: true
  scope: selection_or_block
  profile: brace_semicolon
  block_style: braces
  statement_style: semicolon
  brace_style: attach
  one_statement_per_line: true
  space_before_block_opener: true
  space_after_comma: true
  space_after_control_keyword: true
  space_around_binary_operators: true
  space_around_logical_operators: true
  logical_operators: ["&&", "||"]
  binary_operators: ["+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">="]
  unary_prefix_operators: ["!", "~"]
  protected_scopes: [comment, string, character, preprocessor]
  pointer_alignment: name
  continuation_indent: 4
  max_column: 100
  wrap_after_comma: true
  case_indent: 0
  preserve_blank_lines: true
  max_blank_lines: 0
  collapse_empty_blocks: false
```

## How `Ctrl+J` chooses what to format

`Ctrl+J` never formats the whole file by default.

With:

```yaml
scope: selection_or_block
```

Graptoς behaves like this:

- If text is selected, only the selected text is formatted.
- If nothing is selected, Graptoς finds the surrounding `{ ... }` block or
  function around the cursor and formats that range.
- If no surrounding block exists, no text is changed.

The edit is applied as one undoable buffer action.

## Generic backend model

Graptoς uses one formatter backend. It should not grow language-specific
functions such as `format_c()` or `format_python()`.

Language differences belong in syntax YAML:

- `profile` chooses the generic language family.
- `block_style` describes the block model.
- `statement_style` describes how statements normally end.
- operator lists describe which tokens are logical, binary, or unary.
- protected scopes describe which rule scopes must be copied unchanged.

The backend currently executes these profiles:

- `brace_semicolon`
- `brace_optional_semicolon`

The backend accepts these profile names for future generic engines, but leaves
text unchanged until their layout pass is implemented:

- `indent_colon`
- `markup`
- `data`
- `plain`

This matters because formatting Python, YAML, HTML, Markdown, and JSON with a
C-like brace formatter would be unsafe. They still use the same schema, but
their profile engines must be implemented separately inside the shared backend.

## Protected text

The formatter must not structurally edit inside protected lexical regions:

- comments
- strings
- character literals
- preprocessor directives

That means these characters are ignored for structural formatting when they
appear inside protected text:

- `{`
- `}`
- `;`
- `,`
- operators such as `+`, `&&`, `||`

Example:

```c
const char *text = "{ not a block; }";
```

The braces and semicolon inside the string are copied as string text.

## Option reference

### `enabled`

Turns formatting on or off for this language.

```yaml
enabled: true
```

If this is `false`, `Ctrl+J` returns without modifying text.

### `scope`

Controls the range formatted by `Ctrl+J`.

```yaml
scope: selection_or_block
```

Current supported value:

- `selection_or_block`

### `profile`

Chooses the generic formatter family.

```yaml
profile: brace_semicolon
```

Supported schema values:

- `brace_semicolon`
- `brace_optional_semicolon`
- `indent_colon`
- `markup`
- `data`
- `plain`

Currently implemented formatting behavior exists for:

- `brace_semicolon`
- `brace_optional_semicolon`

Use `brace_semicolon` for languages where semicolons normally terminate
statements, such as C, Java, PHP, and Zig.

Use `brace_optional_semicolon` for brace languages where semicolons are optional
or not normally used, such as JavaScript, TypeScript, Go, and Rust.

### `block_style`

Describes how the language represents nested blocks.

```yaml
block_style: braces
```

Supported schema values:

- `braces`
- `indent`
- `tags`
- `none`

Current formatter execution supports `braces`.

### `statement_style`

Describes how complete statements are normally recognized.

```yaml
statement_style: semicolon
```

Supported schema values:

- `semicolon`
- `optional_semicolon`
- `newline`
- `none`

The actual terminator tokens still come from:

```yaml
statement_required_enders:
```

Do not duplicate statement terminators inside `formatting:`.

### `brace_style`

Controls where opening braces are placed.

```yaml
brace_style: attach
```

Supported values:

`attach`

```c
if (ready) {
    run();
}
```

`allman`

```c
if (ready)
{
    run();
}
```

`stroustrup`

```c
if (ready) {
    run();
}
else {
    stop();
}
```

### `one_statement_per_line`

Splits complete statements onto separate physical lines.

```yaml
one_statement_per_line: true
```

Example:

```c
int a = 1; int b = 2;
```

becomes:

```c
int a = 1;
int b = 2;
```

Semicolons inside `for (...)` are protected by parenthesis depth:

```c
for (guint i = 0; i < count; i++) {
    process(i);
}
```

The two semicolons in the `for` header do not create new lines.

### `space_before_block_opener`

Adds a space before block openers such as `{`.

```yaml
space_before_block_opener: true
```

Example:

```c
if (ready){
```

becomes:

```c
if (ready) {
```

### `space_after_comma`

Adds one space after commas.

```yaml
space_after_comma: true
```

Example:

```c
foo(a,b,c);
```

becomes:

```c
foo(a, b, c);
```

If `wrap_after_comma` also applies, the comma may be followed by a newline
instead of a space.

### `space_after_control_keyword`

Controls spacing after control keywords such as `if`, `for`, `while`, and
`switch`.

```yaml
space_after_control_keyword: true
```

Intended result:

```c
if(ready)
```

becomes:

```c
if (ready)
```

Current note: this is the policy flag. Some cases may be handled indirectly by
the general spacing pass until the formatter grows a fuller token model.

### `space_around_binary_operators`

Adds spaces around common arithmetic, comparison, assignment, and bitwise binary
operators.

```yaml
space_around_binary_operators: true
```

Example:

```c
a=b+c;
```

becomes:

```c
a = b + c;
```

This option covers operators such as:

- `=`
- `+`
- `-`
- `*`
- `/`
- `%`
- `<`
- `>`
- `<=`
- `>=`
- `==`
- `!=`
- `&`
- `|`
- `^`

Pointer `*` is handled separately by `pointer_alignment`.

Logical `&&` and `||` are handled separately by
`space_around_logical_operators`.

The exact binary tokens may be declared with:

```yaml
binary_operators: ["+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">="]
```

If this list is omitted, Graptoς keeps the C-like fallback operator set for
older syntax files.

### `space_around_logical_operators`

Adds spaces around binary logical operators.

```yaml
space_around_logical_operators: true
```

Example:

```c
if(a&&b||!c) {
}
```

becomes:

```c
if(a && b || !c) {
}
```

This option applies to:

- `&&`
- `||`

The exact logical tokens come from:

```yaml
logical_operators: ["&&", "||"]
```

Languages with word operators can declare them too:

```yaml
logical_operators: ["and", "or"]
```

Word operators are matched only on identifier boundaries. Graptoς must not treat
`and` inside `android` or `or` inside `orange` as an operator.

Unary logical-not stays tight:

```c
!ready
```

Graptoς does not format it as:

```c
! ready
```

This separation lets you keep arithmetic compact while making boolean
expressions readable.

### `unary_prefix_operators`

Declares prefix operators that should stay attached to the operand.

```yaml
unary_prefix_operators: ["!", "~"]
```

Example:

```c
!ready
```

The formatter should not change that into:

```c
! ready
```

Only declare operators that are safely prefix-only for the language. Do not add
ambiguous tokens such as `*` or `&` unless the shared formatter has enough
context to distinguish unary use from multiplication, pointer, address, or
reference syntax.

### `protected_scopes`

Lists semantic syntax-rule scopes that must be copied without internal
formatting.

```yaml
protected_scopes: [comment, string, character, preprocessor]
```

The formatter uses the existing syntax `rules:` and their optional `scope:`
fields as the contract. Rule names are not supposed to be the long-term API.

Current supported protected scopes:

- `comment`
- `string`
- `character`
- `preprocessor`

Older syntax files without `scope:` still receive conservative protection from
common comment and quote handling, but new syntax definitions should add
semantic scopes.

### `pointer_alignment`

Controls spacing around `*` in pointer declarations.

```yaml
pointer_alignment: name
```

Supported values:

`name`

```c
char *text;
```

`type`

```c
char* text;
```

`middle`

```c
char * text;
```

The formatter uses heuristics so it does not blindly rewrite multiplication or
dereference expressions.

### `continuation_indent`

Defines the intended indentation width for wrapped or continued expressions.

```yaml
continuation_indent: 4
```

Current note: comma-wrapped lines use one continuation indentation level based
on the active editor indentation settings. The numeric value is parsed and kept
in the syntax model, but wrapping is still basic.

Example shape:

```c
some_function(first_argument,
    second_argument);
```

### `max_column`

Sets the preferred maximum line length before wrapping is considered.

```yaml
max_column: 100
```

Current behavior:

- `0` disables column-based wrapping.
- Values above `0` allow wrapping decisions.
- Graptoς does not wrap every long line yet.
- Current wrapping is tied to `wrap_after_comma`.

### `wrap_after_comma`

Allows wrapping after commas once the current line reaches or exceeds
`max_column`.

```yaml
wrap_after_comma: true
```

Example:

```c
gboolean fn(Type *a, const char *b, gsize *c) {
```

may become:

```c
gboolean fn(Type *a, const char *b,
    gsize *c) {
```

This is intentionally conservative. It only wraps at comma points. It does not
yet do full expression wrapping, alignment, or clang-format style bin packing.

### `case_indent`

Controls the intended indentation adjustment for `case` and `default` labels
inside `switch`.

```yaml
case_indent: 0
```

`0` means case labels should stay aligned with the switch body style.

Current note: switch and case formatting is still basic.

### `preserve_blank_lines`

Controls whether blank lines from the original code are kept.

```yaml
preserve_blank_lines: true
```

If this is `false`, blank lines are generally collapsed or removed during
formatting.

If this is `true`, blank lines are preserved subject to `max_blank_lines`.

Blank lines immediately before a block closer are dropped because they usually
come from previous layout noise:

```c
{
    run();

}
```

becomes:

```c
{
    run();
}
```

### `max_blank_lines`

Controls how many consecutive blank lines may be preserved.

```yaml
max_blank_lines: 0
```

Current behavior:

- `0` means unlimited blank lines.
- `1` means preserve at most one consecutive blank line.
- Higher values preserve up to that number of consecutive blank lines.

For no extra blank lines:

```yaml
preserve_blank_lines: false
```

For at most one blank line:

```yaml
preserve_blank_lines: true
max_blank_lines: 1
```

### `collapse_empty_blocks`

Controls whether empty blocks may be collapsed.

```yaml
collapse_empty_blocks: false
```

Current note: the field is parsed and stored, but empty-block collapsing is not
fully implemented yet. Keep it `false` unless the formatter grows explicit
empty-block handling.

## Current limits

The formatter is intentionally not a full `clang-format` replacement yet.

Current first-pass limits:

- No AST rewriting.
- No include sorting.
- No declaration alignment.
- No full line wrapping.
- No macro-body formatting.
- No full switch/case reconstruction.
- Non-brace profiles are schema-recognized but not formatting-enabled yet.
- Pointer formatting is heuristic.
- `space_after_control_keyword` is policy-level and may need stronger token
  handling for every edge case.
