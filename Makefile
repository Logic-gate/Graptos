APP_NAME := cleaf
APP_ID := io.github.cleaf.Editor
VERSION := 0.16.3
PREFIX ?= /usr/local
CC ?= cc
BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests
DATADIR := $(PREFIX)/share/cleaf

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
	$(SRC_DIR)/index.c \
	$(SRC_DIR)/project.c \
	$(SRC_DIR)/project_search.c \
	$(SRC_DIR)/git.c \
	$(SRC_DIR)/codex_protocol.c \
	$(SRC_DIR)/codex_client.c \
	$(SRC_DIR)/codex_panel.c \
	$(SRC_DIR)/codex_review.c \
	$(SRC_DIR)/codex_markdown.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/syntax.c \
	$(SRC_DIR)/syntax_loader.c \
	$(SRC_DIR)/syntax_highlight.c \
	$(SRC_DIR)/syntax_diagnostics.c \
	$(SRC_DIR)/dialogs.c

ifeq ($(BUILD),debug)
BUILD_DIR := build/debug
CFLAGS ?= -O0 -g3 -pipe
CPPFLAGS += -DCLEAF_DEBUG=1
STRIP := :
else
CFLAGS ?= -O2 -g0 -pipe
STRIP := strip --strip-unneeded
endif
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CPPFLAGS += -DAPP_VERSION=\"$(VERSION)\" -DDATADIR=\"$(DATADIR)\"
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

GTK_PKG := gtk4 gtksourceview-5 json-glib-1.0
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(GTK_PKG))
GTK_SYSTEM_CFLAGS := $(foreach flag,$(GTK_CFLAGS),$(if $(filter -I%,$(flag)),-isystem $(patsubst -I%,%,$(flag)),$(flag)))
GTK_LIBS := $(shell $(PKG_CONFIG) --libs $(GTK_PKG))
LDFLAGS += -Wl,--as-needed

.PHONY: all clean install uninstall size check test debug dist

all: $(BUILD_DIR)/$(APP_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(GTK_SYSTEM_CFLAGS) -std=c11 -c $< -o $@

$(BUILD_DIR)/$(APP_NAME): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(GTK_LIBS) -o $@
	$(STRIP) $@ || true

check:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(GTK_SYSTEM_CFLAGS) -std=c11 -fsyntax-only $(SOURCES)

test: $(BUILD_DIR)/unit_tests
	$<

$(BUILD_DIR)/unit_tests: $(TEST_DIR)/unit_tests.c $(SRC_DIR)/codex_protocol.c $(SRC_DIR)/syntax_diagnostics.c | $(BUILD_DIR)
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
	install -Dm644 data/$(APP_ID).desktop $(DESTDIR)$(PREFIX)/share/applications/$(APP_ID).desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(APP_NAME)
	rm -f $(DESTDIR)$(PREFIX)/share/applications/$(APP_ID).desktop
	rm -rf $(DESTDIR)$(DATADIR)

clean:
	rm -rf $(BUILD_DIR)

dist:
	cd .. && tar -czf cleaf-text-editor-$(VERSION).tar.gz cleaf-$(VERSION)
