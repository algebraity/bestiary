.DEFAULT_GOAL := all

PLATFORM ?= linux
RELEASE_NAME ?= bestiary
RELEASE_SUFFIX := $(if $(VERSION),-$(VERSION),)
CFLAGS   ?= -Wall -Wextra -O2
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
LDFLAGS  ?=
DEPFLAGS  = -MMD -MP
SANITIZE_ENABLED := $(strip $(findstring -fsanitize,$(CFLAGS) $(CXXFLAGS) $(LDFLAGS)))

ifeq ($(PLATFORM),windows)
  ifeq ($(origin CC),default)
    CC := x86_64-w64-mingw32-gcc
  endif
  ifeq ($(origin CXX),default)
    CXX := x86_64-w64-mingw32-g++
  endif
  WINDRES ?= x86_64-w64-mingw32-windres
  IMAGEMAGICK ?= $(shell command -v magick 2>/dev/null || command -v convert 2>/dev/null)
	ifneq ($(filter undefined default,$(origin WX_CONFIG)),)
		ifneq ($(wildcard /usr/x86_64-w64-mingw32/bin/wx-config),)
			WX_CONFIG := /usr/x86_64-w64-mingw32/bin/wx-config
		else
			WX_CONFIG := x86_64-w64-mingw32-wx-config
		endif
	endif
  EXEEXT := .exe
  OBJDIR := build/obj/windows
  BINDIR := build/bin/windows
	DISTDIR := build/dist/windows
	WINDOWS_DLL_DIR := /usr/x86_64-w64-mingw32/bin
	WINDOWS_RUNTIME_DLLS := \
		libexpat-1.dll \
		libjpeg-8.dll \
		libpng16-16.dll \
		libtiff-6.dll \
		zlib1.dll \
		libgcc_s_seh-1.dll \
		libwinpthread-1.dll \
		libssp-0.dll \
		liblzma-5.dll
	WINDOWS_RUNTIME_BINS := $(addprefix $(BINDIR)/,$(WINDOWS_RUNTIME_DLLS))
	WINDOWS_BUNDLE_DIR := $(DISTDIR)/bestiary
	WINDOWS_RELEASE_STEM := $(RELEASE_NAME)$(RELEASE_SUFFIX)-windows
	WINDOWS_RELEASE_DIR := $(DISTDIR)/$(WINDOWS_RELEASE_STEM)
	WINDOWS_RELEASE_ARCHIVE := $(DISTDIR)/$(WINDOWS_RELEASE_STEM).zip
	WINDOWS_INSTALLER_FILES := packaging/windows/install.bat packaging/windows/README-WINDOWS.txt
	CPPFLAGS += -Iinclude -Ithird_party/linenoise -DBST_PLATFORM_WINDOWS -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00
  LDFLAGS += -static -static-libgcc
  LDLIBS := -lm
  GUI_LIBS :=
  GUI_LDFLAGS_EXTRA := -mwindows
  WINDOWS_RC_SRC := packaging/windows/bestiary.rc
  WINDOWS_MANIFEST := packaging/windows/bestiary.manifest
  WINDOWS_ICON_PNG := art/icon.png
  WINDOWS_ICON_ICO := $(OBJDIR)/bestiary.ico
  WINDOWS_RC_OBJ := $(OBJDIR)/bestiary_resources.o
  LINE_INPUT_SRCS := src/platform/line_input_linenoise.c third_party/linenoise/linenoise.c
else
  ifeq ($(origin CC),default)
    CC := gcc
  endif
  ifeq ($(origin CXX),default)
    CXX := g++
  endif
  WX_CONFIG ?= wx-config
  EXEEXT :=
	ifneq ($(SANITIZE_ENABLED),)
		OBJDIR := build/obj/linux-sanitize
		BINDIR := build/bin/linux-sanitize
	else
		OBJDIR := build/obj/linux
		BINDIR := build/bin/linux
	endif
	DISTDIR := build/dist/linux
	LINUX_RELEASE_STEM := $(RELEASE_NAME)$(RELEASE_SUFFIX)-linux
	LINUX_RELEASE_DIR := $(DISTDIR)/$(LINUX_RELEASE_STEM)
	LINUX_RELEASE_ARCHIVE := $(DISTDIR)/$(LINUX_RELEASE_STEM).tar.gz
	WINDOWS_RUNTIME_BINS :=
	WINDOWS_RELEASE_DIR :=
	WINDOWS_RELEASE_ARCHIVE :=
	WINDOWS_INSTALLER_FILES :=
  CPPFLAGS += -Iinclude
  LDLIBS := -lm -lreadline
  GUI_LIBS := -lutil
  GUI_LDFLAGS_EXTRA :=
  WINDOWS_RC_OBJ :=
  WINDOWS_ICON_ICO :=
  LINE_INPUT_SRCS := src/platform/line_input_readline.c
endif

BEAST_SRCS = \
	src/beasts/hebi.c \
	src/beasts/sokko.c \
	src/beasts/usagi.c \
	src/beasts/poni.c \
	src/beasts/ookami.c \
	src/beasts/tora.c \
	src/beasts/neko.c

CORE_SRCS = \
	src/core/lexer.c \
	src/core/ast.c \
	src/core/parser.c \
	src/core/script.c \
	src/core/value.c \
	src/core/eval.c

REPL_SRC = src/repl.c
GUI_SRC = src/gui/bestiary_gui.cpp

TEST_SRCS = \
	tests/test_sokko.c \
	tests/test_usagi.c \
	tests/test_poni.c \
	tests/test_ookami.c \
	tests/test_tora.c \
	tests/test_neko.c

BEAST_OBJS = $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(BEAST_SRCS))
CORE_OBJS = $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(CORE_SRCS))
REPL_OBJ = $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(REPL_SRC))
GUI_OBJ = $(patsubst src/%.cpp,$(OBJDIR)/src/%.o,$(GUI_SRC))
LINE_INPUT_OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(LINE_INPUT_SRCS))
TEST_OBJS = $(patsubst tests/%.c,$(OBJDIR)/tests/%.o,$(TEST_SRCS))

CLI_BIN := $(BINDIR)/bestiary-cli$(EXEEXT)
REPL_BIN := $(BINDIR)/repl$(EXEEXT)
GUI_BIN := $(BINDIR)/bestiary$(EXEEXT)
TEST_BINS = \
	$(BINDIR)/test_sokko$(EXEEXT) \
	$(BINDIR)/test_usagi$(EXEEXT) \
	$(BINDIR)/test_poni$(EXEEXT) \
	$(BINDIR)/test_ookami$(EXEEXT) \
	$(BINDIR)/test_tora$(EXEEXT) \
	$(BINDIR)/test_neko$(EXEEXT)

DEPFILES = $(BEAST_OBJS:.o=.d) $(CORE_OBJS:.o=.d) $(REPL_OBJ:.o=.d) $(GUI_OBJ:.o=.d) $(LINE_INPUT_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
BUILD_CONFIG_STAMP := $(OBJDIR)/.build-config

.PHONY: all bestiary bestiary-cli cli repl gui linux linux-gui windows windows-gui windows-bundle bundle release release-gui linux-release linux-release-package windows-release windows-release-package release-artifacts beasts pipeline tests clean FORCE

all: bestiary

bestiary: $(GUI_BIN) $(WINDOWS_RUNTIME_BINS)

bestiary-cli cli: $(CLI_BIN)

repl: $(REPL_BIN)

gui: bestiary

linux:
	$(MAKE) PLATFORM=linux bestiary repl

linux-gui:
	$(MAKE) PLATFORM=linux gui

windows:
	$(MAKE) PLATFORM=windows bestiary repl

windows-gui:
	$(MAKE) PLATFORM=windows gui

windows-bundle:
	$(MAKE) PLATFORM=windows bundle

release: linux windows

release-gui: linux-gui windows-gui

linux-release:
	$(MAKE) PLATFORM=linux RELEASE_NAME="$(RELEASE_NAME)" VERSION="$(VERSION)" linux-release-package

linux-release-package: $(LINUX_RELEASE_ARCHIVE)

windows-release:
	$(MAKE) PLATFORM=windows RELEASE_NAME="$(RELEASE_NAME)" VERSION="$(VERSION)" windows-release-package

windows-release-package: $(WINDOWS_RELEASE_ARCHIVE)

release-artifacts: linux-release windows-release

bundle: $(WINDOWS_BUNDLE_DIR)

beasts: $(BEAST_OBJS)

pipeline: $(CORE_OBJS) $(REPL_OBJ) $(LINE_INPUT_OBJS)

tests: $(TEST_BINS)

$(CLI_BIN): $(CORE_OBJS) $(REPL_OBJ) $(LINE_INPUT_OBJS) $(BEAST_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(REPL_BIN): $(CLI_BIN) | $(BINDIR)
	cp $< $@

src/gui/embedded_icon.h: art/icon.png tools/embed_png.sh
	./tools/embed_png.sh art/icon.png src/gui/embedded_icon.h BestiaryEmbeddedIcon kIconBase64

$(GUI_OBJ): src/gui/embedded_icon.h

$(GUI_BIN): $(GUI_OBJ) $(WINDOWS_RC_OBJ) $(CLI_BIN) | $(BINDIR)
	@command -v $(WX_CONFIG) >/dev/null 2>&1 || { echo "wxWidgets config tool not found: $(WX_CONFIG)"; echo "Install wxWidgets development packages or set WX_CONFIG=/path/to/wx-config."; exit 1; }
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(GUI_LDFLAGS_EXTRA) -o $@ $(GUI_OBJ) $(WINDOWS_RC_OBJ) `$(WX_CONFIG) --libs` $(GUI_LIBS)

$(WINDOWS_BUNDLE_DIR): FORCE $(CLI_BIN) $(REPL_BIN) $(GUI_BIN) $(WINDOWS_RUNTIME_BINS) $(WINDOWS_INSTALLER_FILES)
	@if [ "$(PLATFORM)" != "windows" ]; then \
		echo "bundle is only supported with PLATFORM=windows"; \
		exit 1; \
	fi
	rm -rf $@
	@mkdir -p $@
	cp $(CLI_BIN) $(REPL_BIN) $(GUI_BIN) $(WINDOWS_RUNTIME_BINS) $@/
	cp $(WINDOWS_INSTALLER_FILES) $@/

$(LINUX_RELEASE_DIR): FORCE $(CLI_BIN) $(GUI_BIN)
	@if [ "$(PLATFORM)" != "linux" ]; then \
		echo "linux release packaging is only supported with PLATFORM=linux"; \
		exit 1; \
	fi
	rm -rf $@
	@mkdir -p $@
	cp $(CLI_BIN) $(GUI_BIN) $@/

$(LINUX_RELEASE_ARCHIVE): $(LINUX_RELEASE_DIR)
	cd $(DISTDIR) && tar -czf $(notdir $@) $(notdir $(LINUX_RELEASE_DIR))

$(WINDOWS_RELEASE_DIR): FORCE $(CLI_BIN) $(GUI_BIN) $(WINDOWS_RUNTIME_BINS)
	@if [ "$(PLATFORM)" != "windows" ]; then \
		echo "windows release packaging is only supported with PLATFORM=windows"; \
		exit 1; \
	fi
	rm -rf $@
	@mkdir -p $@
	cp $(CLI_BIN) $(GUI_BIN) $(WINDOWS_RUNTIME_BINS) $@/

$(WINDOWS_RELEASE_ARCHIVE): $(WINDOWS_RELEASE_DIR)
	cd $(DISTDIR) && zip -rq $(notdir $@) $(notdir $(WINDOWS_RELEASE_DIR))

ifeq ($(PLATFORM),windows)
$(WINDOWS_ICON_ICO): $(WINDOWS_ICON_PNG)
	@mkdir -p $(dir $@)
	@if [ -z "$(IMAGEMAGICK)" ]; then \
		echo "ImageMagick (magick or convert) is required to build the Windows icon"; \
		echo "Install imagemagick or set IMAGEMAGICK=/path/to/magick"; \
		exit 1; \
	fi
	$(IMAGEMAGICK) "$<" -define icon:auto-resize=256,128,96,64,48,32,16 "$@"

$(WINDOWS_RC_OBJ): $(WINDOWS_RC_SRC) $(WINDOWS_ICON_ICO)
	@mkdir -p $(dir $@)
	$(WINDRES) -I packaging/windows -I $(dir $(WINDOWS_ICON_ICO)) -i $(WINDOWS_RC_SRC) -O coff -o $@
endif

$(BINDIR)/%.dll: | $(BINDIR)
	@if [ "$(PLATFORM)" != "windows" ]; then \
		echo "Windows runtime DLL staging is only supported with PLATFORM=windows"; \
		exit 1; \
	fi
	@if [ ! -f "$(WINDOWS_DLL_DIR)/$*.dll" ]; then \
		echo "missing Windows runtime DLL: $(WINDOWS_DLL_DIR)/$*.dll"; \
		exit 1; \
	fi
	cp "$(WINDOWS_DLL_DIR)/$*.dll" "$@"

$(BINDIR)/test_sokko$(EXEEXT): $(OBJDIR)/tests/test_sokko.o $(OBJDIR)/src/beasts/sokko.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

$(BINDIR)/test_usagi$(EXEEXT): $(OBJDIR)/tests/test_usagi.o $(OBJDIR)/src/beasts/usagi.o | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

$(BINDIR)/test_poni$(EXEEXT): $(OBJDIR)/tests/test_poni.o $(OBJDIR)/src/beasts/poni.o $(OBJDIR)/src/beasts/sokko.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

$(BINDIR)/test_ookami$(EXEEXT): $(OBJDIR)/tests/test_ookami.o $(OBJDIR)/src/beasts/ookami.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

$(BINDIR)/test_tora$(EXEEXT): $(OBJDIR)/tests/test_tora.o $(OBJDIR)/src/beasts/tora.o $(OBJDIR)/src/beasts/sokko.o $(OBJDIR)/src/beasts/usagi.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

$(BINDIR)/test_neko$(EXEEXT): $(OBJDIR)/tests/test_neko.o $(OBJDIR)/src/beasts/neko.o | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

$(BUILD_CONFIG_STAMP): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' \
		'PLATFORM=$(PLATFORM)' \
		'CC=$(CC)' \
		'CXX=$(CXX)' \
		'CPPFLAGS=$(CPPFLAGS)' \
		'CFLAGS=$(CFLAGS)' \
		'CXXFLAGS=$(CXXFLAGS)' \
		'LDFLAGS=$(LDFLAGS)' \
		'LDLIBS=$(LDLIBS)' \
		'WX_CONFIG=$(WX_CONFIG)' > $@.tmp
	@if [ ! -f $@ ] || ! cmp -s $@.tmp $@; then \
		mv $@.tmp $@; \
	else \
		rm -f $@.tmp; \
	fi

$(OBJDIR)/%.o: %.c $(BUILD_CONFIG_STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp $(BUILD_CONFIG_STAMP)
	@command -v $(WX_CONFIG) >/dev/null 2>&1 || { echo "wxWidgets config tool not found: $(WX_CONFIG)"; echo "Install wxWidgets development packages or set WX_CONFIG=/path/to/wx-config."; exit 1; }
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) `$(WX_CONFIG) --cxxflags` $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BINDIR):
	@mkdir -p $@

clean:
	rm -rf build

-include $(DEPFILES)
