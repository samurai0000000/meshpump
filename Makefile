# Makefile
#
# Copyright (C) 2025, Charles Chiou

ARCH :=		$(shell uname -m)
MAKEFLAGS =	--no-print-dir
CMAKE :=	$(shell [ -x /usr/bin/cmake ] && echo /usr/bin/cmake || which cmake)
NUM_PROCS :=	$(shell nproc 2>/dev/null || echo 4)

ifneq ($(strip $(CFLAGS)$(CXXFLAGS)$(LDFLAGS)),)
BUILD_DIR :=	build-sanitized/$(ARCH)
CMAKE_SAN_FLAGS := -DCMAKE_C_FLAGS="$(CFLAGS)" -DCMAKE_CXX_FLAGS="$(CXXFLAGS)" -DCMAKE_EXE_LINKER_FLAGS="$(LDFLAGS)"
else
BUILD_DIR :=	build/$(ARCH)
CMAKE_SAN_FLAGS :=
endif

TARGETS :=	$(BUILD_DIR)/meshpump

.PHONY: all default meshpump test clean distclean release install

all: default

default: $(TARGETS)

meshpump: $(TARGETS)

test: all
	@$(CMAKE) --build $(BUILD_DIR) --target test -j$(NUM_PROCS)

clean:
	@test -f $(BUILD_DIR)/Makefile && $(MAKE) -C $(BUILD_DIR) clean || true

distclean:
	rm -rf build/ build-sanitized/

MESHPUMP_TREE :=	\
	CMakeLists.txt version.h.in \
	$(wildcard *.c) $(wildcard *.cxx) $(wildcard *.h) $(wildcard *.hxx) \
	libmeshtastic font8x8 test
MESHPUMP_SRCS :=	$(shell find -H $(MESHPUMP_TREE) -type f \
	    \( -name '*.c' -o -name '*.cxx' -o -name '*.h' -o -name '*.hxx' \
	       -o -name 'CMakeLists.txt' -o -name 'version.h.in' \) \
	    2>/dev/null)

$(TARGETS) &: $(BUILD_DIR)/Makefile $(MESHPUMP_SRCS)
	@$(MAKE) -C $(BUILD_DIR) -j$(NUM_PROCS)

$(BUILD_DIR)/Makefile: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) ../.. $(CMAKE_SAN_FLAGS)

release: $(BUILD_DIR)/Makefile
	@cd $(BUILD_DIR) && $(CMAKE) ../.. $(CMAKE_SAN_FLAGS)
	@$(MAKE) -C $(BUILD_DIR) -j$(NUM_PROCS)

install: release
	@sudo install -m 755 $(BUILD_DIR)/meshpump /usr/local/bin/meshpump
	@sudo strip /usr/local/bin/meshpump
