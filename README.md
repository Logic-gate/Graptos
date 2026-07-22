![cleaf](https://i.snipboard.io/1oBVMY.jpg) 

# Graptoς _Previously Cleaf_

γραπτός: Grap-Tos; adj, written or inscribed

Graptoς is a small text editor for Linux, written in pure C with GTK 4 and
GtkSourceView 5. It is still under active development and is not ready for
production use.

It provides the tools expected from a modern editor, including project
navigation, completion, formatting, Git support, previews, optional LSP
integration, and an optional Codex assistant. Much of its behavior can be
configured through YAML language definitions.

## Aim

The aim of Graptoς is to sit somewhere between a basic text editor and a full
IDE. It should provide the tools used during everyday development without
becoming large, complicated, or tied to one workflow.

Graptoς takes inspiration from editors such as Sublime Text and nano. Sublime
shows the value of speed, keyboard-driven editing, and practical project tools.
Nano shows that an editor can remain direct and useful without requiring much
setup. Graptoς follows the same general idea: add features when they make editing
or navigation easier, but avoid turning the editor into an environment that
tries to manage everything.

Low memory usage and a small dependency set remain important goals. Graptoς
should be easy to build, understandable to contributors, and usable on modest
systems. New features must justify their runtime, dependency, and maintenance
costs.

## Features

* Pure C codebase with a GTK 4 interface
* GtkSourceView 5 editor core with optional Graptoς YAML highlighting
* Tabbed editing
* Project folder sidebar with live refresh and Git file indicators
* Optional split and tiled editing for up to three tabs
* Toggleable minimap and scroll preview
* Markdown and LaTeX previews that can be resized or detached
* Project-wide literal search and replacement
* Manual completion with `Ctrl+Space`
* Optional automatic completion
* LSP completion, diagnostics, references, and go-to-definition when a
  configured language server is available
* Graptoς YAML fallback completion, references, import completion, and project
  indexing when LSP is unavailable
* C and C++ clangd fallback include flags derived from syntax `import_roots`
* Reference and definition panel shown by holding `Alt` while hovering over or
  selecting text
* Syntax-aware formatting with `Ctrl+J`
* Regex tester popover for selected regex-like text
* Integrated terminal with single-terminal and dynamic-directory modes
* Init Project dialog for generating projects from declarative templates
* Live theme editor with previews and persistent `config.ini` support
* Config migration that adds new keys without replacing existing values
* Bundled themes including Dracula, GitHub Dark, Apprentice, Gruvbox Dark,
  Tokyo Night, Srcery, and Moonfly
* Integrated Codex panel with streaming chat, editor context, approval controls,
  and per-turn change review
* Git status, diffs, staging, commits, pulls, pushes, and credential-helper
  support
* YAML-configurable highlighting, completion, formatting, imports, LSP commands,
  and project indexing

## Limitations

Graptoς is still early software. Some parts are incomplete or deliberately
limited:

* The theme editor is live and persistent, but not every widget can be themed
  yet.
* Plugin support is not available. Graptoς can currently be extended through
  configuration files, but there is no stable plugin API.
* Some UI details still need work, especially around edge cases, panel
  behavior, and less common GTK themes.
* Automated testing is growing, but it does not yet cover every editor workflow.
* Graptoς prefers LSP results when available and otherwise falls back to bounded
  local indexing. The local index is useful, but it is heuristic and is not a
  compiler.
* C and C++ `import_roots` are passed to clangd as fallback include flags. A
  project `compile_commands.json` or `compile_flags.txt` is still the correct
  source for exact compiler arguments.
* The formatter understands language structure, but it is not a replacement for
  tools such as `clang-format`, `rustfmt`, or `gofmt`.
* Graptoς currently targets Linux and depends on GTK 4. Support for other
  platforms is not a priority yet.
* The AI panel requires the external `codex` CLI and only works when it is
  installed and configured separately.

## Requirements

Building Graptoς requires:

* A C11 compiler
* GTK 4.10 or later
* GtkSourceView 5
* JSON-GLib 1.0
* VTE for the integrated terminal
* `pkg-config`
* `make`

The optional AI panel also requires the `codex` CLI to be installed, available
on `PATH`, and configured. Graptoς starts it locally with `codex app-server --stdio`. The editor does not store API keys or call an AI provider directly.

## Build and run

Clone the repository, then run:

```bash
make
./build/graptos
```

To enable Graptoς runtime logging and a native crash backtrace, run:

```bash
./build/graptos debug
```

To install Graptoς system-wide:

```bash
make
sudo make install
```

The default installation prefix is `/usr/local`. It can be changed with
`PREFIX`:

```bash
make install PREFIX="$HOME/.local"
```

## Theme and preferences

Open `File > Theme` to change Graptoς colors and fonts. Changes are previewed
inside the dialog before they are saved. The theme editor writes to:

```text
~/.config/graptos/config.ini
```

The file can also be edited manually. Use `Reload config.ini` in the Theme
dialog to apply manual changes without restarting Graptoς. The current config
keys are documented in [docs/PREFERENCES.md](docs/PREFERENCES.md).

When a newer build introduces config keys, Graptoς adds only the missing keys to
an existing `config.ini`. Existing values are not changed. If the file does not
exist, Graptoς creates it from the current defaults on first launch.

Bundled themes are loaded from `data/themes/` in the source tree and from the
installed theme directory after `make install`.

## Keyboard shortcuts

Open `File > Shortcuts` for the full in-app list. Common shortcuts include:

| Shortcut            | Action                                    |
| ------------------- | ----------------------------------------- |
| `Ctrl+N` / `Ctrl+T` | New tab                                   |
| `Ctrl+O`            | Open file                                 |
| `Ctrl+Shift+O`      | Open folder                               |
| `Ctrl+S`            | Save                                      |
| `Ctrl+Shift+S`      | Save as                                   |
| `Ctrl+W`            | Close active tab                          |
| `Ctrl+B`            | Toggle project tree                       |
| `` Ctrl+` ``        | Toggle terminal panel                     |
| `Ctrl+Shift+I`      | Toggle AI panel                           |
| `Ctrl+F`            | Find                                      |
| `Ctrl+H`            | Replace                                   |
| `Ctrl+G`            | Go to line                                |
| `Ctrl+J`            | Format selected code or surrounding block |
| `Ctrl+Space`        | Show completion                           |

## Git support

Open a folder inside a Git repository, then select `Git` from the bottom bar.
Graptoς supports:

* Repository status and manual refresh
* File-change markers in the project tree
* Diff tabs
* Staging or unstaging the current file
* Staging all changes
* Committing staged changes
* Pulling with `--ff-only`
* Pushing
* HTTPS credentials through the configured Git credential helper
* Generic Git commands using parsed arguments rather than shell execution

GitHub, GitLab, Gitea, and other remotes work through the normal Git CLI. For
GitHub over HTTPS, use a personal access token instead of an account password.
SSH remotes use the system SSH configuration and `ssh-agent`.

Git operations do not run in the editor's typing path. Repository status is
refreshed when a folder is opened, a file is saved, a refresh is requested, or
a Git command finishes.

## LSP and local code intelligence

Graptoς can use the Language Server Protocol when a syntax definition provides
an LSP command. LSP results are preferred for:

* Completion
* Diagnostics
* References
* Go to definition

When LSP is unavailable or does not return a useful result, Graptoς falls back
to its YAML-driven completion, import completion, reference lookup, and project
index. Completion results are grouped by source so LSP and Graptoς suggestions
remain easy to distinguish.

For C and C++, Graptoς passes syntax `import_roots` and `import_env` to clangd
through `initializationOptions.fallbackFlags` when the syntax definition uses:

```yaml
import_style: c_include
lsp_command: clangd
```

This gives clangd fallback `-I...` paths for small projects and individual
files. A real `compile_commands.json` is still preferred and should contain the
same flags used by the build. This is especially important for libraries such
as GTK, where `pkg-config --cflags gtk4` expands to several include paths.

Local indexing is limited by file size, project size, and the syntax `index`
setting. Graptoς does not execute project code to generate suggestions.

See [docs/SYNTAX.md](docs/SYNTAX.md) for the complete syntax and LSP contract.

## Formatting

Press `Ctrl+J` to format selected code. If there is no selection and the active
language supports it, Graptoς formats the surrounding block.

Formatting is configured through the optional `formatting:` section in syntax
YAML files. It reuses existing language fields such as `indent_openers`,
`indent_closers`, `statement_required_enders`, `line_comment`, and syntax rule
scopes. Languages without formatting rules are left unchanged.

See [docs/FORMATTING.md](docs/FORMATTING.md) for the available options and current limits.

## Terminal panel

Open `View > Terminal` or press `` Ctrl+` `` to show the terminal. It starts in
the open project directory or the active file's directory.

`View > Terminal Mode` provides two modes:

* Single: one terminal session.
* Dynamic Directory: one terminal tab for each active file directory.

Terminal tab names can be edited. Closing a terminal tab asks the shell to exit
and removes the tab once the child process can be released safely.

## Tiled editing

Shift-click editor tabs to create a tiled view. The number of visible tiles is
limited by `tile_max_tabs` in the config and defaults to three. Each tile scrolls
independently, while the minimap follows the active tile.

A tile group can be saved as one grouped tab through the tiled-tab context menu.
Tabs can later be detached from the group.

Grouped tile tabs are saved groups rather than ordinary editor tabs. Once a
group has been saved, it opens from its grouped tab instead of being tiled
again.

## Project templates

Open `File > Init Project` to create a project from a declarative template.
Templates are loaded from the installed data directory and from:

```text
~/.config/graptos/project-templates/
```

Each template contains a `template.yaml` manifest and its source files. The Init
Project dialog shows the description, collects template variables, previews the
files that will be generated, and then writes them. It does not run arbitrary
shell commands.

See [docs/PROJECT_TEMPLATES.md](docs/PROJECT_TEMPLATES.md) for the manifest format.

## Codex AI assistant

Select `AI` from the bottom bar or press `Ctrl+Shift+I` to open the Codex panel.
The panel starts the local Codex CLI in the open project directory. If no project
is open, it uses Graptoς's current directory.

The panel supports:

* Streaming responses and activity updates
* Multiple chat tabs backed by separate Codex threads
* Starting new chats and resuming a thread when switching tabs
* Stopping an active turn
* Optional context from the active file or current selection
* Allow-once and deny controls for command and file-change requests
* A list of files changed during each turn
* Opening the turn's diff in a read-only editor tab
* Keeping or reverting only the changes made during that turn

File and selection context are enabled by default and can be disabled before a
prompt is sent. Context is read from the current in-memory buffer, so it may
include unsaved edits. It is limited to 64 KiB per prompt.

When Codex asks to run a command or change files, Graptoς waits for an explicit
decision in the panel. After a turn changes files, review the recorded diff
before selecting `Keep` or `Revert`.

Reverting uses `git apply --reverse` and first checks whether the patch can still
be applied safely. Git is therefore required, and a revert may fail if the files
have changed since the turn was completed.

## Markdown and LaTeX preview

Select `View > Preview` to open a preview for supported Markdown and LaTeX
documents.

Markdown is rendered in a read-only preview buffer. The preview can be resized
inside the main window, detached into a separate flat Graptoς window, and
reattached later. Closing a detached preview window disables preview for that
tab.

For LaTeX, Graptoς searches for an external engine in this order:

```text
pdflatex
xelatex
lualatex
```

Set `GRAPTOS_LATEX_COMMAND` to use a specific engine:

```bash
GRAPTOS_LATEX_COMMAND=xelatex ./build/graptos
```

Graptoς runs the command directly without using a shell. The default arguments
are:

```text
-interaction=nonstopmode
-halt-on-error
-file-line-error
-no-shell-escape
-output-directory .graptos-latex-build
```

For saved documents, generated files are written to:

```text
<source-folder>/.graptos-latex-build/
```

For an unsaved buffer, Graptoς creates and renders a temporary `document.tex`.

## Import completion

Import completion is static and bounded. Graptoς does not run Python, Node,
shells, compilers, package managers, or project code to generate suggestions.

Python completion sources may include:

* The current file's directory
* The open project root
* Project virtual environments
* `PYTHONPATH`
* `VIRTUAL_ENV`
* `CONDA_PREFIX`
* Common user and system `site-packages` paths
* Package `__init__.py` and `__init__.pyi` files
* `.py`, `.pyi`, `.so`, and `.pyd` package members
* Common built-in modules such as `sys`, `os`, and `math`
* Static typeshed `stdlib` locations, when available

Press `Ctrl+Space` to request suggestions manually.

## Project search

Open a project folder, then select `Edit > Project Search`.

Search and replacement are literal, bounded, and limited to the open project.
Graptoς only scans language definitions with `index: true`, which avoids
searching unrelated binary files.

## Backup and temporary files

Graptoς uses atomic temporary files while saving and removes them after a
successful save. When backups are enabled, they are stored in
`.graptos-backups/` instead of beside the original file as `filename~`.

These paths should normally be added to the project's `.gitignore`:

```gitignore
.graptos-backups/
.graptos-save-*.tmp
.graptos-latex-build/
```

## Nano feature map

Graptoς provides several editing actions that will be familiar to nano users,
but through a GTK interface.

| Area       | Nano action          | Graptoς action                                      |
| ---------- | -------------------- | --------------------------------------------------- |
| File       | New buffer           | `File > New`                                        |
| File       | Read file            | `File > Open`                                       |
| File       | Write out            | `File > Save` or `File > Save As`                   |
| File       | Exit                 | `File > Quit`, with an unsaved-changes prompt       |
| Edit       | Cut text             | `Ctrl+X` or right-click                             |
| Edit       | Copy text            | `Ctrl+C` or right-click                             |
| Edit       | Paste text           | `Ctrl+V` or right-click                             |
| Edit       | Cut current line     | `Ctrl+K`                                            |
| Edit       | Uncut text           | `Ctrl+U`                                            |
| Edit       | Undo                 | `Ctrl+Z`                                            |
| Edit       | Redo                 | `Ctrl+Y`                                            |
| Search     | Where Is             | `Ctrl+F`                                            |
| Search     | Find next            | `F3`                                                |
| Search     | Find previous        | `Shift+F3`                                          |
| Search     | Replace              | `Ctrl+H`                                            |
| Navigation | Go to line           | `Ctrl+G`                                            |
| Formatting | Format code          | `Ctrl+J`                                            |
| Commenting | Comment or uncomment | `Ctrl+/`, using `line_comment` from the syntax YAML |

## YAML language definitions

Language definitions control highlighting, completion, file badges, formatting,
import behavior, LSP commands, and project indexing.

The complete syntax and LSP behavior is documented in [docs/SYNTAX.md](docs/SYNTAX.md).

### Example

```yaml
name: TypeScript
extensions: [.ts, .tsx, .mts, .cts]
filenames: [vite.config.ts]
line_comment: "//"
icon: TS
index: true
completions: [import, export, const, let, function, interface, type]
rules:
  - name: keyword
    pattern: "(?<![A-Za-z0-9_])(import|export|const|let|function|interface|type)(?![A-Za-z0-9_])"
    color: "#569CD6"
    bold: true
```

### Exact filenames

Use `filenames` for files without an extension or for names that need exact
matching:

```yaml
name: Makefile
extensions: []
filenames: [Makefile, GNUmakefile]
icon: MK
index: true
```

### Excluding files from project indexing

Set `index: false` for large or noisy file types that should still be
highlighted but should not appear in completion or reference results:

```yaml
name: Large Log
extensions: [.log]
icon: LOG
index: false
```

### AI Policy

AI contributions are based on [AI USAGE POLICY VERSION 1](https://github.com/Logic-gate/AI-USAGE-POLICY/blob/main/POLICY/VERSION_1.0.0.md) [docs/AI_USAGE_POLICY.md](docs/AI_USAGE_POLICY.md).
