![cleaf](https://i.snipboard.io/hdrCL5.jpg) 

# Cleaf [![C/C++ CI](https://github.com/Logic-gate/cleaf/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/Logic-gate/cleaf/actions/workflows/c-cpp.yml) 

Cleaf is a small, pure-C text editor for Linux. It is under active development and is not yet ready for
production use.

Cleaf combines familiar text-editing tools with YAML-configurable syntax
highlighting, completion, file badges, import completion, project indexing,
and an optional Codex coding assistant.

## AIM

Cleaf is meant to be a modern toolkit for developers, without trying to become
a full IDE. It should offer the everyday tools programmers actually need while
staying centered on fast text editing, quick project navigation, and a clear,
understandable interface.

The project takes inspiration from lightweight editors like Sublime Text and
nano. From Sublime, Cleaf borrows the emphasis on speed, keyboard-driven
workflows, and practical project navigation. From nano, it takes the value of
being direct, small, and useful without requiring much setup. Cleaf follows that
same idea: add focused features where they make development easier, but avoid
becoming an environment that tries to control the entire workflow.

Low memory usage and minimal dependencies are core goals. Cleaf should stay
easy to build, easy to understand, and practical to run on modest systems.
Every feature should earn its place by being worth its runtime cost, dependency
cost, and maintenance cost, so the editor can grow without losing what makes it
lightweight.

_You can track the development of Cleaf here_ [Cleaf Development](https://github.com/users/Logic-gate/projects/2).

To get a sense of what is instored for cleaf, please visit [Milestones](https://github.com/Logic-gate/cleaf/milestones).

Cleaf is being developed as a lightweight, reliable, and extensible text editor for Linux, written in C with GTK4.

The project focuses first on a stable and responsive editing core, clear project navigation, strong file handling, and consistent native integration.
More advanced capabilities, such as AI assistants, platform-specific tooling, build systems, and Sailfish OS support, will be provided through pluginsrather than being built directly into the core. The long-term goal is not to turn Cleaf into a full traditional IDE, but to provide a fast editor that can gain IDE-like functionality only where the user needs it.

## Features

- Pure C codebase with a GTK 4 interface
- GtkSourceView 5 editor core with optional Cleaf YAML highlighting
- Tabbed editing
- Project folder sidebar
- Toggleable scroll preview
- Real-time Markdown preview
- Manual LaTeX rendering
- Project-wide literal search and replacement
- Manual completion with `Ctrl+Space`
- Optional automatic completion
- Reference panel shown by holding `Alt` while hovering over or selecting text
- Integrated Codex panel with streaming chat, editor context, approval controls,
  and per-turn change review
- Git status, diffs, staging, commits, pulls, pushes, and credential-helper
  support
- YAML-configurable syntax highlighting, completion, and project indexing

## Limitations

Cleaf is still early software, so several areas are intentionally limited or
unfinished:

- The theming engine is basic. Themes are currently configured through
  `~/.config/cleaf/config.ini` rather than a full in-app theme editor.
- Plugin support is not available yet. Cleaf can be extended through its
  existing configuration files, but it does not have a stable plugin API.
- Some UI details still need polish, especially around edge cases, panel
  behavior, and less common GTK themes.
- Testing is still growing. Core behavior has tests, but the project does not
  yet have broad automated coverage for every editor workflow.
- Completion, project indexing, and reference lookup are static and bounded.
  They are useful for lightweight workflows, but they are not a replacement for
  a full language server.
- Cleaf currently targets Linux and depends on GTK 4, so portability to other
  platforms is not a primary focus yet.
- The AI panel depends on the external `codex` CLI and only works when that
  tool is installed and configured separately.

## Requirements

Building Cleaf requires:

- A C11 compiler
- GTK 4.10 or later
- GtkSourceView 5
- JSON-GLib 1.0
- `pkg-config`
- `make`

The optional AI panel requires the `codex` CLI to be installed, available on
`PATH`, and configured for use. Cleaf starts it locally as `codex app-server
--stdio`; the editor does not embed API keys or call an AI provider directly.

## Build and run

Clone the repository, then run:

```bash
make
./build/cleaf
```

To update, pull the repository, then:

```bash
make clean
make
./build/cleaf
```

To install Cleaf system-wide:

```bash
make
sudo make install
```

To update a system wide install, pull the repository, then:

```bash
sudo make uninstall
make clean
make
sudo make install
```

The default installation prefix is `/usr/local`. You can override it with
`PREFIX`, for example:

```bash
make install PREFIX="$HOME/.local"
```

## Git support

Open a folder inside a Git repository and select `Git` from the bottom bar.
Cleaf supports:

- Repository status and refresh
- File-change markers in the project tree
- Diff tabs
- Staging or unstaging the current file
- Staging all changes
- Committing staged changes
- Pulling with `--ff-only`
- Pushing
- HTTPS credentials through Git's configured credential helper
- Generic Git commands using parsed arguments instead of shell execution

GitHub, GitLab, Gitea, and other Git remotes work through the normal Git CLI.
For GitHub over HTTPS, use a personal access token instead of an account
password. SSH remotes use the system SSH configuration and `ssh-agent`.

Git commands do not run in the editor's typing path. Status refreshes when a
folder is opened, a file is saved, a refresh is requested, or a Git command
finishes.

## Codex AI assistant

Select `AI` in the bottom bar or press `Ctrl+Shift+I` to toggle the Codex
panel. The panel starts the local Codex CLI in the open project's directory,
or in Cleaf's current directory when no project is open.

The panel supports:

- Streaming Codex responses and activity updates
- Multiple chat tabs backed by separate Codex threads
- New chats and resuming a thread when switching tabs
- Stopping an active turn
- Optional context from the active file or current selection
- Explicit allow-once or deny controls for command and file-change requests
- A list of files changed during a turn
- Opening the turn's diff in a read-only editor tab
- Keeping the changes or reverting only that turn

File and selection context are enabled by default and can be disabled before
sending a prompt. Context is taken from the current in-memory buffer, so it can
include unsaved edits, and is capped at 64 KiB per prompt.

When Codex requests permission to run a command or change files, Cleaf waits
for an explicit decision in the panel. After a turn changes files, review the
recorded diff before choosing `Keep` or `Revert`. Reverting uses `git apply
--reverse` and first checks that the patch can still be applied safely; it
therefore requires Git and can fail if the affected files have changed since
the turn.

## LaTeX rendering

Select `View > Render LaTeX` to render the current document. Cleaf searches
for the following commands in order:

```text
pdflatex
xelatex
lualatex
```

Set `CLEAF_LATEX_COMMAND` to choose a specific engine:

```bash
CLEAF_LATEX_COMMAND=xelatex ./build/cleaf
```

Cleaf executes the command directly without invoking a shell. The default
arguments are:

```text
-interaction=nonstopmode
-halt-on-error
-file-line-error
-no-shell-escape
-output-directory .cleaf-latex-build
```

For a saved document, generated files are placed in:

```text
<source-folder>/.cleaf-latex-build/
```

For an unsaved buffer, Cleaf creates and renders a temporary `document.tex`.

## Import completion

Import completion is static and bounded. Cleaf does not execute Python, Node,
shells, compilers, package managers, or project code to generate suggestions.

Python completion sources can include:

- The current file's directory
- The open project root
- Project virtual environments
- `PYTHONPATH`
- `VIRTUAL_ENV`
- `CONDA_PREFIX`
- Common user and system `site-packages` paths
- Package `__init__.py` and `__init__.pyi` files
- `.py`, `.pyi`, `.so`, and `.pyd` package members
- Common built-in modules such as `sys`, `os`, and `math`
- Static typeshed `stdlib` locations, when available

Press `Ctrl+Space` to request suggestions manually.

## Project search

Open a project folder, then select `Tools > Project Search`.

Search and replacement are literal, bounded, and limited to the open project.
Cleaf uses language definitions with `index: true` to avoid scanning unrelated
binary files.

## Backup and temporary files

Cleaf uses atomic temporary files when saving and removes them after a
successful save. When backups are enabled, they are stored in
`.cleaf-backups/` rather than beside the edited file as `filename~`.

Consider adding these paths to your project's `.gitignore`:

```gitignore
.cleaf-backups/
.cleaf-save-*.tmp
.cleaf-latex-build/
```

## Nano feature map

Cleaf provides many practical editing actions familiar to nano users through
a GTK interface.

| Area | Nano action | Cleaf action |
|---|---|---|
| File | New buffer | `File > New` or `Ctrl+N`|
| File | Read file | `File > Open` or `Ctrl+O`|
| File | Write out | `File > Save` or `File > Save As` or `Ctrl+S`|
| File | Exit | `File > Quit`, with an unsaved-changes prompt |
| Edit | Cut text | `Ctrl+X` or right-click |
| Edit | Copy text | `Ctrl+C` or right-click |
| Edit | Paste text | `Ctrl+V` or right-click |
| Edit | Cut current line | `Ctrl+K` |
| Edit | Uncut text | `Ctrl+U` |
| Edit | Undo | `Ctrl+Z` |
| Edit | Redo | `Ctrl+Y` |
| Search | Where Is | `Ctrl+F` |
| Search | Find next | `F3` |
| Search | Find previous | `Shift+F3` |
| Search | Replace | `Ctrl+H` |
| Navigation | Go to line | `Ctrl+G` |
| Formatting | Justify paragraph | `Ctrl+J` |
| Commenting | Comment or uncomment | `Ctrl+/`, using `line_comment` from the syntax YAML |

## YAML language definitions

Language definitions control syntax highlighting, completion, file badges,
import behavior, and project indexing.

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

Use `filenames` for files without an extension or for names that require exact
matching:

```yaml
name: Makefile
extensions: []
filenames: [Makefile, GNUmakefile]
icon: MK
index: true
```

### Excluding files from project indexing

Set `index: false` for large or noisy file types that should be highlighted
but should not contribute to completion or reference results:

```yaml
name: Large Log
extensions: [.log]
icon: LOG
index: false
```


## Screenshots
Markdown Preview
![markdown preview](https://i.snipboard.io/pDesxH.jpg)
LaTeX
![latex shortcuts](https://i.snipboard.io/sFwmPQ.jpg)
Ref & Def
![ref](https://i.snipboard.io/hNaXZc.jpg)
Auto-complete
![autocomplete](https://i.snipboard.io/GRmzwT.jpg)
git
![git](https://i.snipboard.io/fReSDU.jpg)
Codex Approval
![codex1](https://i.snipboard.io/qiUWgn.jpg)
Codex Changes Approval
![codex2](https://i.snipboard.io/jwJQ1L.jpg)
Codex Diff View
![codex3](https://i.snipboard.io/uWe9Gq.jpg)

