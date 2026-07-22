# Graptoς syntax and LSP

Graptoς syntax files are YAML language definitions. They describe editor
behavior that must work even when no language server is installed.

Language servers add semantic knowledge. They do not replace syntax YAML.

## Where syntax files live

Bundled syntax files live in:

```text
syntax/
```

User syntax files can live in the user config area:

```text
~/.config/graptos/syntax/
```

Project-specific syntax files can live under a project-local Graptoς config
directory when project support needs that.

## How Graptoς chooses a syntax

Graptoς matches a file using the syntax definition fields:

```yaml
name: C
extensions:
  - .c
  - .h
filenames: []
```

`extensions` match file suffixes.

`filenames` match exact basenames such as `Makefile`, `build.zig`, or
`CMakeLists.txt`.

The selected syntax definition then controls highlighting, indentation,
formatting, import completion, local references, diagnostics hints, and LSP
startup.

## Syntax YAML responsibilities

These fields are editor-local. They work without LSP:

```yaml
line_comment: //
auto_indent: true
indent_openers: ["{"]
indent_closers: ["}"]
statement_required_enders: [";"]
statement_exempt_prefixes: [...]
statement_exempt_suffixes: [...]
rules: [...]
close_pairs: [...]
line_close_pairs: [...]
completions: [...]
formatting: ...
```

### Highlighting

`rules` are regex-based syntax rules. A rule can also carry a semantic `scope`:

```yaml
- name: string
  scope: string
  pattern: "(?<![A-Za-z0-9_])\"(?:\\\\.|[^\"\\\\])*\""
  color: "#CE9178"
```

Graptoς currently uses scopes such as:

```text
comment
string
character
preprocessor
```

These scopes help the formatter avoid changing protected regions.

### Local completion

`completions` are static Graptoς suggestions.

They are always local and do not require LSP:

```yaml
completions:
  - const
  - return
  - while
```

### Import completion

Import completion uses:

```yaml
import_style: c_include
import_keywords: ["#include"]
import_roots:
  - .
  - /usr/include
  - /usr/include/gtk-4.0
import_env:
  - CPATH
  - C_INCLUDE_PATH
import_extensions:
  - .h
  - .hpp
```

`import_roots` are search roots used by Graptoς to suggest importable files.

Relative roots are resolved against the project root and current file
directory. Absolute roots are used as written. Environment variables listed in
`import_env` are split using the system path separator.

## LSP responsibilities

LSP is configured per syntax:

```yaml
lsp_command: clangd
lsp_args: []
lsp_language_id: c
```

`lsp_command` starts the server.

`lsp_args` are passed directly to the server process.

`lsp_language_id` is sent in `textDocument/didOpen`.

When LSP is available, Graptoς asks it for semantic features:

- autocomplete
- go to definition
- references
- diagnostics, warnings, and errors

Graptoς still keeps local YAML/index behavior as fallback. If LSP is missing,
slow, or returns no useful result, local Graptoς completions and references can
still appear.

## LSP priority and fallback behavior

Graptoς treats LSP as the first semantic source and YAML as the stable local
source.

For autocomplete:

1. Graptoς asks LSP when a server is configured and the file is saved.
2. LSP candidates are marked as `LSP`.
3. Local YAML/import/index candidates are marked as `Graptoς`.
4. The completion popover can show both sources.

For references and definitions:

1. Graptoς requests LSP definition/reference locations.
2. Graptoς also keeps local index/reference behavior available.
3. The ref/def popover can show both LSP and Graptoς results.

For diagnostics:

1. Graptoς receives `textDocument/publishDiagnostics` from the language server.
2. Diagnostics are rendered in the editor.
3. `diagnostics_enabled=false` in `config.ini` disables diagnostic rendering.

## C and C++ include paths with clangd

clangd does not understand Graptoς `import_roots` directly.

clangd normally finds include paths from:

```text
compile_commands.json
compile_flags.txt
.clangd
compiler defaults
```

Graptoς now bridges C/C++ syntax import metadata to clangd by sending clangd
`initializationOptions.fallbackFlags`.

For a C syntax definition like:

```yaml
import_style: c_include
lsp_command: clangd
import_roots:
  - .
  - /usr/include
  - /usr/include/gtk-4.0
import_env:
  - CPATH
  - C_INCLUDE_PATH
```

Graptoς sends clangd fallback flags similar to:

```text
-I/project/root
-I/usr/include
-I/usr/include/gtk-4.0
```

This is only a fallback. A real project compile database remains the better
source of truth.

Use `compile_commands.json` when possible because it captures exact compiler
flags, defines, standards, generated include paths, and per-file settings.

The fallback bridge is intended for small projects, single files, and projects
that do not yet have a compile database.

## Why `#include <gtk/gtk.h>` can fail

Having this in YAML:

```yaml
import_roots:
  - /usr/include/gtk-4.0
```

means Graptoς can search that directory for import completion.

clangd still needs compiler include flags. Graptoς now mirrors those C/C++
roots into clangd fallback flags, but GTK usually needs more include paths than
only GTK itself.

For GTK4, the complete compiler flags usually come from:

```sh
pkg-config --cflags gtk4
```

Those flags include GTK, GLib, Pango, Cairo, GDK-Pixbuf, HarfBuzz, and platform
include directories.

If clangd still warns after the fallback bridge is active, create a proper
`compile_commands.json` or `compile_flags.txt` for the project using the same
flags your build uses.

## Recommended C/C++ project setup

Preferred:

```text
compile_commands.json
```

Acceptable for small projects:

```text
compile_flags.txt
```

Fallback:

```yaml
import_roots:
  - /usr/include
  - /usr/include/gtk-4.0
import_env:
  - CPATH
  - C_INCLUDE_PATH
  - CPLUS_INCLUDE_PATH
```

The fallback keeps basic diagnostics usable, but it cannot fully replace the
real compiler command.

## Debugging LSP

Run Graptoς in debug mode:

```sh
./build/graptos debug
```

Useful LSP messages include:

```text
LSP[...] started command=...
LSP[...] send initialize ...
LSP[...] clangd fallbackFlags from import_roots count=...
LSP[...] publishDiagnostics uri=... count=...
LSP[...] completion request ...
```

If the fallback bridge is active for clangd, debug output should show:

```text
clangd fallbackFlags from import_roots count=N
```

If that line does not appear, check:

- `lsp_command` is `clangd`
- `import_style` is `c_include`
- `import_roots` contains existing directories
- the active file actually matched the C or C++ syntax definition
