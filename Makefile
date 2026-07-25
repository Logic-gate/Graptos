APP_NAME := graptos
APP_ID := io.github.graptos.Editor
VERSION := 0.23.6
PREFIX ?= /usr/local
CC ?= cc
BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests
DATADIR := $(PREFIX)/share/graptos

SOURCES := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/app_core.c \
	$(SRC_DIR)/app_layout.c \
	$(SRC_DIR)/app_actions.c \
	$(SRC_DIR)/app_state.c \
	$(SRC_DIR)/ui.c \
	$(SRC_DIR)/ui_css.c \
	$(SRC_DIR)/editor_tab_core.c \
	$(SRC_DIR)/editor_tab_build.c \
	$(SRC_DIR)/editor_tab_sourceview.c \
	$(SRC_DIR)/editor_tab_io.c \
	$(SRC_DIR)/editor_tab_undo.c \
	$(SRC_DIR)/editor_tab_gutter.c \
	$(SRC_DIR)/editor_tab_preview.c \
	$(SRC_DIR)/editor_tab_latex.c \
	$(SRC_DIR)/editor_tab_hover.c \
	$(SRC_DIR)/editor_tab_completion.c \
	$(SRC_DIR)/editor_tab_keys.c \
	$(SRC_DIR)/editor_tab_diagnostics.c \
	$(SRC_DIR)/editor_tab_edit.c \
	$(SRC_DIR)/completion.c \
	$(SRC_DIR)/import_complete.c \
	$(SRC_DIR)/import_complete_resolve.c \
	$(SRC_DIR)/formatter.c \
	$(SRC_DIR)/formatter_lexer.c \
	$(SRC_DIR)/formatter_layout.c \
	$(SRC_DIR)/formatter_spacing.c \
	$(SRC_DIR)/formatter_scope.c \
	$(SRC_DIR)/index.c \
	$(SRC_DIR)/project.c \
	$(SRC_DIR)/project_init.c \
	$(SRC_DIR)/project_init_ui.c \
	$(SRC_DIR)/project_search.c \
	$(SRC_DIR)/git.c \
	$(SRC_DIR)/codex_protocol.c \
	$(SRC_DIR)/codex_client.c \
	$(SRC_DIR)/codex_panel.c \
	$(SRC_DIR)/codex_review.c \
	$(SRC_DIR)/codex_markdown.c \
	$(SRC_DIR)/terminal_panel.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/lsp_client.c \
	$(SRC_DIR)/syntax.c \
	$(SRC_DIR)/syntax_loader.c \
	$(SRC_DIR)/syntax_highlight.c \
	$(SRC_DIR)/syntax_diagnostics.c \
	$(SRC_DIR)/dialogs.c

APP_SOURCES := $(filter-out $(SRC_DIR)/main.c,$(SOURCES))

ifeq ($(BUILD),debug)
BUILD_DIR := build/debug
CFLAGS ?= -O0 -g3 -pipe
CPPFLAGS += -DGRAPTOS_DEBUG=1
STRIP := :
else
CFLAGS ?= -O2 -g0 -pipe
STRIP := strip --strip-unneeded
endif
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJECTS:.o=.d)
VERSION_HEADER := $(BUILD_DIR)/version.h
CPPFLAGS += -I$(BUILD_DIR) -DDATADIR=\"$(DATADIR)\" -DVTE_DISABLE_DEPRECATED
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2 -Wundef -Wstrict-prototypes -Wmissing-prototypes
PKG_CONFIG ?= pkg-config

ifeq ($(shell $(PKG_CONFIG) --atleast-version=4.10 gtk4 && echo yes),yes)
$(info GTK 4 version check passed.)
else
$(error GTK 4.10 or higher is required but not detected.)
endif

ifeq ($(shell $(PKG_CONFIG) --exists gtksourceview-5 && echo yes),yes)
$(info GtkSourceView 5 check passed.)
else
$(error GtkSourceView 5 is required but not detected. Install gtksourceview-5 development files.)
endif

GTK_PKG := gtk4 gtksourceview-5 json-glib-1.0 vte-2.91-gtk4
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(GTK_PKG))
GTK_SYSTEM_CFLAGS := $(foreach flag,$(GTK_CFLAGS),$(if $(filter -I%,$(flag)),-isystem $(patsubst -I%,%,$(flag)),$(flag)))
GTK_LIBS := $(shell $(PKG_CONFIG) --libs $(GTK_PKG))
LDFLAGS += -Wl,--as-needed

.PHONY: all clean install uninstall size check test smoke-test debug dist

all: $(BUILD_DIR)/$(APP_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(VERSION_HEADER): Makefile | $(BUILD_DIR)
	{ \
		printf '%s\n' '/**'; \
		printf '%s\n' ' * @file version.h'; \
		printf '%s\n' ' * @brief Generated Graptoς version definition.'; \
		printf '%s\n' ' * @details This file is generated from Makefile VERSION so the'; \
		printf '%s\n' ' *          application has one version source of truth.'; \
		printf '%s\n' ' */'; \
		printf '%s\n' '#ifndef GRAPTOS_VERSION_H'; \
		printf '%s\n' '#define GRAPTOS_VERSION_H'; \
		printf '%s\n' '#define APP_VERSION "$(VERSION)"'; \
		printf '%s\n' '#endif'; \
	} > $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(VERSION_HEADER) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(GTK_SYSTEM_CFLAGS) -std=c11 -MMD -MP -c $< -o $@

$(BUILD_DIR)/$(APP_NAME): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(GTK_LIBS) -o $@
	$(STRIP) $@ || true

check: $(VERSION_HEADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(GTK_SYSTEM_CFLAGS) -std=c11 -fsyntax-only $(SOURCES)

test: $(BUILD_DIR)/unit_tests
	$<

$(BUILD_DIR)/unit_tests: $(TEST_DIR)/unit_tests.c $(SRC_DIR)/codex_protocol.c $(SRC_DIR)/syntax_diagnostics.c $(SRC_DIR)/project_init.c $(SRC_DIR)/formatter.c $(SRC_DIR)/formatter_lexer.c $(SRC_DIR)/formatter_layout.c $(SRC_DIR)/formatter_spacing.c $(SRC_DIR)/formatter_scope.c $(SRC_DIR)/syntax.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(GTK_SYSTEM_CFLAGS) -std=c11 $^ $(GTK_LIBS) -o $@

smoke-test: $(BUILD_DIR)/smoke_window
	$<

$(BUILD_DIR)/smoke_window: $(TEST_DIR)/smoke_window.c $(APP_SOURCES) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(GTK_SYSTEM_CFLAGS) -std=c11 $^ $(GTK_LIBS) -o $@

debug:
	$(MAKE) BUILD=debug all

size: $(BUILD_DIR)/$(APP_NAME)
	ls -lh $(BUILD_DIR)/$(APP_NAME)
	stat -c '%s bytes' $(BUILD_DIR)/$(APP_NAME)

install: $(BUILD_DIR)/$(APP_NAME)
	install -Dm755 $(BUILD_DIR)/$(APP_NAME) $(DESTDIR)$(PREFIX)/bin/$(APP_NAME)
	install -d $(DESTDIR)$(DATADIR)/syntax
	install -m644 syntax/*.yaml $(DESTDIR)$(DATADIR)/syntax/
	install -d $(DESTDIR)$(DATADIR)/logos
	install -m644 data/logos/*.png $(DESTDIR)$(DATADIR)/logos/
	install -d $(DESTDIR)$(DATADIR)/themes
	install -m644 data/themes/*.ini $(DESTDIR)$(DATADIR)/themes/
	install -d $(DESTDIR)$(DATADIR)/fonts/Inconsolata
	cp -R data/fonts/Inconsolata/. $(DESTDIR)$(DATADIR)/fonts/Inconsolata/
	install -d $(DESTDIR)$(DATADIR)/project-templates
	cp -R data/project-templates/. $(DESTDIR)$(DATADIR)/project-templates/
	install -Dm644 data/$(APP_ID).desktop $(DESTDIR)$(PREFIX)/share/applications/$(APP_ID).desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(APP_NAME)
	rm -f $(DESTDIR)$(PREFIX)/share/applications/$(APP_ID).desktop
	rm -rf $(DESTDIR)$(DATADIR)

clean:
	rm -rf $(BUILD_DIR)

dist:
	cd .. && tar -czf graptos-text-editor-$(VERSION).tar.gz graptos-$(VERSION)

-include $(DEPS)
