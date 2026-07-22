# Graptoς project templates

Graptoς project templates live under a `project-templates` directory. Each template is a directory containing a `template.yaml` manifest and any source files referenced by that manifest.

The file is named `template.yaml`, but Graptoς currently parses a strict, line-oriented subset of YAML. It is not a full YAML parser. Keep the shape close to the examples below.

## Where templates are loaded from

Graptoς loads templates from these roots:

- Built-in install path: `${DATADIR}/project-templates`
- Source-tree development path: `data/project-templates`
- User templates: `${XDG_CONFIG_HOME}/graptos/project-templates`

User templates override built-in templates when they use the same `id`.

## Directory layout

```text
data/project-templates/example-template/
├── template.yaml
└── files/
    ├── main.c
    └── README.md
```

Only `template.yaml` is special. The `files/` directory is a convention used by the built-in templates.

## Manifest rules

- Comments start with `#` outside quotes.
- Strings may be unquoted, single-quoted, or double-quoted.
- Indentation is significant:
  - top-level keys start at column 0
  - variable names and list items use two spaces
  - fields under a variable/list item use four spaces
- Lists are only supported in two places:
  - `values: [a, b, c]` for choice variables
  - `- path` / `- path: value` entries in `directories`, `files`, and `actions`
- Unknown keys are rejected.
- Paths must be project-relative. Absolute paths, `..`, backslashes, and colon syntax are rejected.

## Top-level keys

Required:

```yaml
version: 1
id: example-template
name: Example Template
language: C
```

Optional:

```yaml
description: Short text shown in the Init Project dialog.
icon: ignored-for-now
category: ignored-for-now
```

`id` must start with a letter or `_`, and may contain letters, numbers, `_`, and `-`.

## Built-in variables

Every template receives these variables:

| Variable | Meaning |
| --- | --- |
| `${project_name}` | Project name typed by the user. |
| `${project_slug}` | Lowercase dash-separated project name. |
| `${project_ident}` | Lowercase underscore-separated project name. |
| `${project_macro}` | Uppercase underscore-separated project name. |
| `${project_dir}` | Final generated project directory. |
| `${author}` | Current real/user name. |
| `${email}` | Empty by default. |
| `${year}` | Current local year. |
| `${license}` | Defaults to `AGPL-3.0`, unless overridden by a template variable. |

Placeholders use this exact form:

```text
${variable_name}
```

Unknown placeholders are errors. Graptoς does not silently leave them in generated files.

## Variables section

Variables define controls shown in the Init Project dialog. They also become placeholders.

Supported variable types:

### Text

```yaml
variables:
  executable_name:
    label: Executable name
    type: text
    default: ${project_slug}
```

Text defaults may reference built-in variables.

### Boolean

```yaml
variables:
  include_tests:
    label: Include tests
    type: boolean
    default: true
```

Boolean defaults must be `true` or `false`.

### Choice

```yaml
variables:
  build_system:
    label: Build system
    type: choice
    values: [make, meson, cmake]
    default: make
```

The default must be one of the listed values.

## Conditions

`when:` may be used on directories, files, and actions.

Supported condition forms:

```yaml
when: include_tests
when: not include_tests
when: build_system == "make"
when: build_system != "cmake"
```

Truth checks treat `true`, `yes`, and `1` as true.

## Directories section

Directories are created under the generated project root.

Short form:

```yaml
directories:
  - src
```

Expanded form with a condition:

```yaml
directories:
  - path: tests
    when: include_tests
```

Directory paths may use placeholders.

## Files section

Each file entry needs:

- `path`: output path inside the generated project
- either `template` or `source`: input file inside the template directory

Rendered template file:

```yaml
files:
  - path: src/main.c
    template: files/main.c
```

Raw copied file:

```yaml
files:
  - path: assets/logo.bin
    source: files/logo.bin
    render: false
```

`template` and `source` both point to files inside the template directory. The difference is intent:

- `template` defaults to rendered output.
- `source` is useful for files copied as-is.
- `render: false` disables placeholder rendering.

## Actions section

Supported actions:

```yaml
actions:
  - type: git-init
    when: initialize_git
```

```yaml
actions:
  - type: mark-executable
    path: scripts/run.sh
```

`mark-executable` requires `path`.

## Open section

The `open` section tells Graptoς what to show after generation.

```yaml
open:
  project: .
  file: src/main.c
```

`project: .` opens the generated project root. `file` is project-relative.

## Minimal complete example

```yaml
version: 1
id: c-minimal
name: C Minimal
description: Small C program with a Makefile.
language: C

variables:
  executable_name:
    label: Executable name
    type: text
    default: ${project_slug}

files:
  - path: main.c
    template: files/main.c

open:
  project: .
  file: main.c
```

## Validation behavior

Graptoς validates a template before it appears in Init Project:

- required top-level fields must exist
- variable names and template id must be valid
- variable types must be known
- choice defaults must be listed in `values`
- file entries must include `path` and `template` or `source`
- referenced template/source files must exist and must not be symlinks
- output paths must stay inside the generated project
- duplicate rendered output paths are rejected
- unsupported actions are rejected

If validation fails, Graptoς skips the template and prints a project-template warning in debug output.
