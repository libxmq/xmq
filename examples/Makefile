# Build and run all the examples against the locally built libxmq.so.
# Run make in the root of the xmq source tree first.

LANGS = $(patsubst %/Makefile,%,$(wildcard */Makefile))

all:
	@for l in $(LANGS); do $(MAKE) -s -C $$l run || exit 1; done

build:
	@for l in $(LANGS); do $(MAKE) -s -C $$l build || exit 1; done

clean:
	@for l in $(LANGS); do $(MAKE) -s -C $$l clean; done

.PHONY: all build clean
