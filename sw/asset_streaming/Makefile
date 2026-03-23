# SC64 Asset Streaming Library - Build System
#
# Targets:
#   make test          - Build and run all host tests
#   make pack          - Pack assets from MANIFEST into ARCHIVE
#   make header        - Generate C header with asset ID #defines
#   make clean         - Remove build artifacts
#
# Examples:
#   make test
#   make pack MANIFEST=game/assets.json ARCHIVE=build/assets.bin
#   make pack ASSET_DIR=game/assets/ ARCHIVE=build/assets.bin
#   make header MANIFEST=game/assets.json HEADER=game/asset_ids.h

SRCDIR    := src
TESTDIR   := tests
TOOLDIR   := tools
BUILDDIR  := build

CC        ?= cc
CFLAGS    := -Wall -Wextra -Wno-unused-parameter -I$(SRCDIR)
PYTHON    ?= python3

# Library sources (for host builds)
LIB_SRC   := $(SRCDIR)/asset_cache.c $(SRCDIR)/lz4_decode.c
DBG_SRC   := $(SRCDIR)/asset_debug.c
MGR_SRC   := $(SRCDIR)/asset_manager.c

# Pack tool defaults
MANIFEST  ?=
ASSET_DIR ?=
ARCHIVE   ?= $(BUILDDIR)/assets.bin
HEADER    ?= $(BUILDDIR)/asset_ids.h
SD_OFFSET ?= 0

$(shell mkdir -p $(BUILDDIR) $(BUILDDIR)/tests)

# =============================================================
# Host tests
# =============================================================

.PHONY: test test-cache test-debug test-integration test-packer test-roundtrip clean pack header

test: test-cache test-debug test-packer test-roundtrip test-integration
	@echo ""
	@echo "=== All test suites passed ==="

test-cache: $(BUILDDIR)/tests/test_cache
	@$<

test-debug: $(BUILDDIR)/tests/test_debug
	@$<

test-integration: $(BUILDDIR)/tests/test_integration $(BUILDDIR)/tests/test_archive.bin
	@cd $(BUILDDIR)/tests && ./test_integration

test-packer:
	@cd $(TESTDIR) && $(PYTHON) test_packer.py

test-roundtrip:
	@cd $(TESTDIR) && $(PYTHON) test_roundtrip.py

# Build rules
$(BUILDDIR)/tests/test_cache: $(TESTDIR)/test_cache.c $(LIB_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/tests/test_debug: $(TESTDIR)/test_debug.c $(LIB_SRC) $(DBG_SRC)
	$(CC) $(CFLAGS) -DASSET_DEBUG -o $@ $^

$(BUILDDIR)/tests/test_integration: $(TESTDIR)/test_integration.c $(MGR_SRC) $(LIB_SRC) $(DBG_SRC)
	$(CC) $(CFLAGS) -DASSET_HOST_BUILD -DASSET_DEBUG -o $@ $^

$(BUILDDIR)/tests/test_archive.bin: $(TESTDIR)/gen_test_archive.py $(TOOLDIR)/asset_pack.py
	@cd $(TESTDIR) && $(PYTHON) gen_test_archive.py
	@cp $(TESTDIR)/test_archive.bin $@

# =============================================================
# Asset packing
# =============================================================

pack:
ifdef ASSET_DIR
	$(PYTHON) $(TOOLDIR)/asset_pack.py --dir $(ASSET_DIR) -o $(ARCHIVE) $(if $(filter-out 0,$(SD_OFFSET)),--sd-offset $(SD_OFFSET))
else ifdef MANIFEST
	$(PYTHON) $(TOOLDIR)/asset_pack.py $(MANIFEST) -o $(ARCHIVE) $(if $(filter-out 0,$(SD_OFFSET)),--sd-offset $(SD_OFFSET))
else
	@echo "Usage: make pack MANIFEST=path/to/manifest.json ARCHIVE=output.bin"
	@echo "   or: make pack ASSET_DIR=path/to/assets/ ARCHIVE=output.bin"
	@exit 1
endif

# =============================================================
# C header generation from manifest
# =============================================================

header:
ifdef MANIFEST
	$(PYTHON) $(TOOLDIR)/asset_pack.py $(MANIFEST) --emit-header $(HEADER)
else ifdef ASSET_DIR
	$(PYTHON) $(TOOLDIR)/asset_pack.py --dir $(ASSET_DIR) --emit-header $(HEADER)
else
	@echo "Usage: make header MANIFEST=path/to/manifest.json HEADER=output.h"
	@exit 1
endif

# =============================================================
# Clean
# =============================================================

clean:
	rm -rf $(BUILDDIR)
	rm -f $(TESTDIR)/test_archive.bin
