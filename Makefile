# Makefile
#
# Copyright (C) 2025, Charles Chiou

ARCH :=		$(shell uname -m)
MAKEFLAGS =	--no-print-dir

TARGETS +=	build/$(ARCH)/meshpump

.PHONY: default clean distclean

default: $(TARGETS)

clean:
	@test -f build/$(ARCH)/Makefile && $(MAKE) -C build/$(ARCH) clean

distclean:
	rm -rf build/

.PHONY: meshpump

meshpump: build/$(ARCH)/meshpump

MESHPUMP_TREE :=	\
	CMakeLists.txt version.h.in \
	$(wildcard *.cxx) $(wildcard *.hxx) \
	libmeshtastic
MESHPUMP_SRCS :=	$(shell find -H $(MESHPUMP_TREE) -type f \
	    \( -name '*.c' -o -name '*.cxx' -o -name '*.h' -o -name '*.hxx' \
	       -o -name 'CMakeLists.txt' -o -name 'version.h.in' \) \
	    2>/dev/null)

build/$(ARCH)/meshpump: build/$(ARCH)/Makefile $(MESHPUMP_SRCS)
	@if [ -f $@ ]; then \
		rm -f build/$(ARCH)/version.h; \
	fi
	@$(MAKE) -C build/$(ARCH)

build/$(ARCH)/Makefile: CMakeLists.txt
	@mkdir -p build/$(ARCH)
	@cd build/$(ARCH) && cmake ../..

.PHONY: release

release: build/$(ARCH)/Makefile
	@rm -f build/$(ARCH)/version.h
	@$(MAKE) -C build/$(ARCH)

.PHONY: install

install: release
	@sudo install -m 755 build/$(ARCH)/meshpump /usr/local/bin/meshpump
	@sudo strip /usr/local/bin/meshpump
