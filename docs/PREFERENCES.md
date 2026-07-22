# Preferences

Graptoς stores preferences in:

```text
~/.config/graptos/config.ini
```

Open the dialog from:

```text
Tools → Theme
```

## Editing

```ini
insert_spaces=true
tab_width=4
autocomplete_enabled=true
diagnostics_enabled=true
auto_save_enabled=false
backup_enabled=false
```

## Config migration

Graptoς updates `config.ini` on startup when a newer build knows about keys
that are missing from the existing file. Existing keys are left unchanged, so
manual edits survive upgrades while new options become visible without deleting
the user config.

If `config.ini` does not exist, Graptoς creates it from the current defaults on
first launch.

## Appearance

Open `File → Theme` to edit individual colors, load bundled presets, or load a
custom theme `.ini` file.

Bundled presets live in:

```text
data/themes/
```

Installed presets live in:

```text
${prefix}/share/graptos/themes/
```

The bundled presets currently are:

- Dracula
- GitHub Dark
- Apprentice
- Gruvbox Dark
- Tokyo Night
- Srcery
- Moonfly

Theme files are normal Graptoς config fragments. They use the `[Editor]` group
and the same color keys as `config.ini`. Loading a preset or custom `.ini`
updates the live preview first; it is written to `config.ini` only when you
press `Apply`.

```ini
[Editor]
use_system_interface_font=false
background_color=#1E1E2E
sidebar_background_color=#242A3D
tabbar_background_color=#1F2333
scroll_preview_background_color=#1B2030
popover_background_color=#242A3D
popover_border_color=#00000000
tooltip_background_color=#242A3D
tooltip_foreground_color=#d4d4d4
tooltip_border_color=#00000000
dialog_background_color=#1b1f24
dialog_foreground_color=#d4d4d4
dialog_border_color=#00000000
dialog_title_color=#ffffff
dialog_body_color=#d4d4d4
dialog_muted_color=#a6adc8
dialog_output_color=#d4d4d4
git_output_background_color=#1b1f24
dialog_action_color=#d4d4d4
dialog_destructive_action_color=#ff6b6b
dialog_input_foreground_color=#d4d4d4
dialog_input_background_color=#181a1f
```

All colour keys are optional. If a colour key is absent, Graptoς does not force a custom background for that area.

Dialog border is retained as a compatibility key. Flat Graptoς dialogs suppress visible borders by default, so normal dialogs use the background and text role keys above.

## Intelligence

```ini
scroll_preview_enabled=true
```

Symbol references use intentional inspection only. Hold `Alt` and hover over a symbol, or hold `Alt` while selecting a symbol or hex colour. Passive hover is not configurable. The popover remains open while the pointer is inside it and hides immediately when the pointer leaves it.
