# Makefile
#
# Copyright (C) 2025, Charles Chiou

ARCH :=		$(shell uname -m)
MAKEFLAGS =	--no-print-dir

TARGETS +=	build/$(ARCH)/meshpump

.PHONY: default clean distclean $(TARGETS)

default: $(TARGETS)

clean:
	@test -f build/$(ARCH)/Makefile && $(MAKE) -C build/$(ARCH) clean

distclean:
	rm -rf build/

.PHONY: meshpump

meshpump: build/$(ARCH)/meshpump

build/$(ARCH)/meshpump: build/$(ARCH)/Makefile
	@$(MAKE) -C build/$(ARCH)

build/$(ARCH)/Makefile: CMakeLists.txt
	@mkdir -p build/$(ARCH)
	@cd build/$(ARCH) && cmake ../..

.PHONY: release

release: build/$(ARCH)/Makefile
	@rm -f build/$(ARCH)/version.h
	@$(MAKE) -C build/$(ARCH)
