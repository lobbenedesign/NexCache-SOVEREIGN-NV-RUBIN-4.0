# NexCache Root Makefile
# ============================================================

all:
	@cd src && $(MAKE) all

install:
	@cd src && $(MAKE) install

test:
	@./runtest

clean:
	@cd src && $(MAKE) clean

distclean:
	@cd src && $(MAKE) distclean

.PHONY: all clean distclean test install
