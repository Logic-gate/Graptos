# Plugins

Graptoς plugins are local folders with a `plugin.yaml` manifest. The plugin
layer is the boundary between Graptoς internals and extensions: plugins declare
what they contribute, and Graptoς decides how those contributions are loaded and
executed.

Version 1 is intentionally small. It supports declarative plugin discovery,
syntax and template directories, command declarations, editor context-menu
items, and a bottom-bar Plugins tool panel. Native plugin loading has an ABI
stub, but Graptoς does not auto-load native code at startup.

## Plugin locations

Graptoς scans these plugin roots:

```text
~/.local/share/graptos/plugins/<plugin-id>/
${prefix}/share/graptos/plugins/<plugin-id>/
data/plugins/<plugin-id>/
```

Use `data/plugins/` while developing inside the source tree. Installed builds
use `${prefix}/share/graptos/plugins/`.

To disable plugin discovery for tests or debugging:

```sh
GRAPTOS_DISABLE_PLUGINS=1 graptos
```

## Basic layout

```text
data/plugins/my-plugin/
  plugin.yaml
  syntax/
  templates/
```

Only `plugin.yaml` is required. Other directories are needed only when the
manifest points at them.

## Manifest

Minimal manifest:

```yaml
id: my-plugin
name: My Plugin
version: 1.0
description: Short description.
graptos_api_version: 1
enabled: true

permissions:
  - editor.read

contributes:
  commands: []
  menus: []
```

Required fields:

- `id`: stable lowercase id. Use lowercase letters, numbers, `.`, `_`, or `-`.
- `name`: visible plugin name.
- `version`: plugin version string.
- `graptos_api_version`: must match the current plugin API version, currently `1`.

Optional fields:

- `description`: short visible description.
- `enabled`: `true` or `false`; defaults to `true`.
- `native`: relative or absolute path to a shared library. Parsed, but not
  auto-loaded during normal startup.
- `permissions`: declared capabilities.
- `contributes`: plugin contribution lists.

## Permissions

Current known permission names:

```text
editor.read
editor.write
project.read
project.write
ui
syntax
theme
template
git
terminal
command
native
```

Graptoς currently enforces permission checks for built-in plugin command
execution where applicable. For example, the Git blame example requires `git`.
Full permission prompt UI is not implemented yet.

## Contributions

### Syntax directories

```yaml
permissions:
  - syntax

contributes:
  syntaxes:
    - syntax
```

Each listed path is resolved relative to the plugin folder unless it is
absolute. The directory should contain Graptoς syntax YAML files.

### Project template directories

```yaml
permissions:
  - template

contributes:
  templates:
    - templates
```

Each listed directory should contain project-template subdirectories. Each
template subdirectory has the same `template.yaml` format used by built-in
project templates.

### Editor commands

Plugins can add editor-line actions. The same action appears in two places:

- The editor right-click menu, where it uses the line under the pointer.
- The bottom-bar Plugins tool panel, where it uses the active cursor line.

```yaml
permissions:
  - editor.read
  - git

contributes:
  commands:
    - git-blame-line
  menus:
    - editor-line:Git Blame This Line:git-blame-line
```

Menu format:

```text
editor-line:<label>:<command-id>
```

- `editor-line` means the item is an editor line command.
- `<label>` is the visible menu text.
- `<command-id>` must also be listed under `contributes.commands`.

Current built-in plugin command ids:

- `git-blame-line`: runs Git blame for the clicked file line and shows the
  result in a dialog.
- `line-word-count`: counts words and characters on the selected line.
- `insert-section-banner`: inserts a language-aware section banner above the
  selected line.
- `project-summary`: shows project root and tab counts.
- `show-plugin-info`: shows metadata for the plugin that owns the command.

## Showcase plugins

The source tree includes one plugin per permission, plus a full demo plugin:

```text
data/plugins/permission-editor-read/
data/plugins/permission-editor-write/
data/plugins/permission-project-read/
data/plugins/permission-project-write/
data/plugins/permission-ui/
data/plugins/permission-syntax/
data/plugins/permission-theme/
data/plugins/permission-template/
data/plugins/permission-git/
data/plugins/permission-terminal/
data/plugins/permission-command/
data/plugins/permission-native/
data/plugins/full-demo/
```

Runtime-visible examples:

- `permission-editor-read`: adds `Count Words On This Line`.
- `permission-editor-write`: adds `Insert Section Banner`.
- `permission-project-read`: adds `Show Project Summary`.
- `permission-ui`: adds `Show Plugin Info`.
- `permission-git`: adds `Git Blame This Line`.
- `run-active-file`: native plugin that adds `Run Active File`.
- `full-demo`: declares every permission and adds `Full Demo Info`.

Declarative asset examples:

- `permission-syntax`: contributes a QML syntax definition.
- `permission-theme`: contributes a CSS theme directory for future theme-pack loading.
- `permission-template`: contributes an Init Project template.
- `permission-project-write`: demonstrates the project-write command shape.
- `permission-terminal`: demonstrates the terminal command shape.
- `permission-command`: demonstrates the external-command shape.
- `permission-native`: demonstrates the native ABI manifest shape.

## Example: Git blame on right-click

The Git example lives in:

```text
data/plugins/permission-git/plugin.yaml
```

Manifest:

```yaml
id: permission-git
name: Git Blame Demo
version: 1.0
description: Adds a Git blame action to the editor line context menu.
graptos_api_version: 1
enabled: true

permissions:
  - editor.read
  - git

contributes:
  commands:
    - git-blame-line
  menus:
    - editor-line:Git Blame This Line:git-blame-line
```

To test it from the right-click menu:

1. Open a file inside a Git repository.
2. Right-click a line in the editor.
3. Choose `Git Blame This Line`.
4. Graptoς runs `git blame -L <line>,<line>` through its internal argv-based Git
   helper and shows the output in a dialog.

The plugin does not run shell text. Graptoς owns the command implementation.

To test it from the Plugins panel:

1. Put the cursor on a line inside a Git-tracked file.
2. Click the bottom-bar Plugins icon.
3. Choose `Git Blame Demo: Git Blame This Line`.
4. Graptoς uses the cursor line as the command target.

## Native plugins

Native plugins are trusted shared libraries loaded after manifest discovery.
The manifest is the reviewable trust layer: it declares identity, API version,
the native library path, and permissions. The `.so` owns the rest: commands,
labels, and behavior.

Native loading rules:

- Graptoς loads only manifests with a `native:` path.
- The manifest must declare the `native` permission.
- Missing native libraries are skipped with a message, which lets source-tree
  example manifests exist before their `.so` is built.
- Existing libraries that fail to load or register are reported as native plugin
  load errors.

Native plugins must export:

```c
gboolean graptos_plugin_register(GraptosPluginHost *host);
```

The public ABI header is:

```text
src/plugin_api.h
```

Current host functions:

```c
guint graptos_plugin_host_api_version(GraptosPluginHost *host);
const char *graptos_plugin_host_plugin_id(GraptosPluginHost *host);
gboolean graptos_plugin_host_register_command(GraptosPluginHost *host,
                                              const char *command_id,
                                              GraptosPluginCommandFunc callback,
                                              gpointer user_data,
                                              GraptosPluginDestroyFunc destroy);
gboolean graptos_plugin_host_register_editor_line_command(GraptosPluginHost *host,
                                                          const char *command_id,
                                                          const char *label,
                                                          GraptosPluginCommandFunc callback,
                                                          gpointer user_data,
                                                          GraptosPluginDestroyFunc destroy);
```

Command context functions:

```c
const char *graptos_plugin_context_plugin_id(GraptosPluginCommandContext *context);
const char *graptos_plugin_context_command_id(GraptosPluginCommandContext *context);
guint graptos_plugin_context_line(GraptosPluginCommandContext *context);
char *graptos_plugin_context_file_path(GraptosPluginCommandContext *context);
char *graptos_plugin_context_text(GraptosPluginCommandContext *context);
char *graptos_plugin_context_selection(GraptosPluginCommandContext *context);
char *graptos_plugin_context_line_text(GraptosPluginCommandContext *context,
                                       guint line);
gboolean graptos_plugin_context_insert_text(GraptosPluginCommandContext *context,
                                            const char *text);
gboolean graptos_plugin_context_replace_selection(GraptosPluginCommandContext *context,
                                                  const char *text);
void graptos_plugin_context_show_output(GraptosPluginCommandContext *context,
                                        const char *title,
                                        const char *heading,
                                        const char *body);
void graptos_plugin_context_set_status(GraptosPluginCommandContext *context,
                                       const char *text);
guint graptos_plugin_context_tab_count(GraptosPluginCommandContext *context);
char *graptos_plugin_context_project_root(GraptosPluginCommandContext *context);
gboolean graptos_plugin_context_open_file(GraptosPluginCommandContext *context,
                                          const char *path);
```

Strings returned as `char *` are owned by the plugin and must be freed with
`g_free()`. Strings returned as `const char *` are owned by Graptoς and are valid
only for the current call.

Minimal native manifest:

```yaml
id: native-line-demo
name: Native Line Demo
version: 1.0
description: Registers an editor line command from a shared library.
graptos_api_version: 1
enabled: true
native: lib/libgraptos_native_demo.so

permissions:
  - native
  - editor.read
  - ui
```

Minimal native source:

```c
#include "plugin_api.h"

static void line_info(GraptosPluginCommandContext *context, gpointer user_data) {
    (void)user_data;
    guint line = graptos_plugin_context_line(context);
    char *text = graptos_plugin_context_line_text(context, line);
    char *body = g_strdup_printf("Line: %u\n\n%s", line, text ? text : "");
    graptos_plugin_context_show_output(context,
                                       "Native Plugin",
                                       "Native Line Info",
                                       body);
    g_free(body);
    g_free(text);
}

gboolean graptos_plugin_register(GraptosPluginHost *host) {
    if (graptos_plugin_host_api_version(host) != GRAPTOS_PLUGIN_API_VERSION) {
        return FALSE;
    }
    return graptos_plugin_host_register_editor_line_command(host,
                                                           "native-line-info",
                                                           "Native Line Info",
                                                           line_info,
                                                           NULL,
                                                           NULL);
}
```

Build shape:

```sh
cd data/plugins/permission-native
mkdir -p lib
cc -fPIC -shared -I../../../src native_demo.c \
  $(pkg-config --cflags --libs glib-2.0) \
  -o lib/libgraptos_native_demo.so
```

The plugin links to GLib and resolves Graptoς ABI symbols from the running
application.

### Example: Run active file

The native run-command example lives in:

```text
data/plugins/run-active-file/
```

It registers `Run Active File` from its `.so`. The plugin runs the active saved
file through a simple extension map: `.py`, `.js`, `.mjs`, `.sh`, `.bash`, `.rb`,
`.pl`, `.lua`, and `.php`. Files with executable permission are run directly.
Process output is captured and shown in a Graptoς dialog.

Build it from the plugin directory:

```sh
cd data/plugins/run-active-file
mkdir -p lib
cc -fPIC -shared -I../../../src run_active_file.c \
  $(pkg-config --cflags --libs glib-2.0) \
  -o lib/libgraptos_run_active_file.so
```

## Design rules

- Plugins do not receive `EditorWindow *`, `EditorTab *`, or GTK internals.
- Manifest ids must be stable because they become config and permission keys.
- YAML owns identity, API version, permissions, and optional static assets.
- Native plugins own behavior, visible command labels, and command callbacks.
- Do not execute shell commands from manifests.
