# Build and run all the C examples against the locally built libxmq.so.
# Run make in the root of the xmq source tree first.

EXAMPLES = $(patsubst %/Makefile,%,$(wildcard */Makefile))

all:
	@for e in $(EXAMPLES); do $(MAKE) -s -C $$e || exit 1; done

run:
	@for e in $(EXAMPLES); do echo "== $$e"; $(MAKE) -s -C $$e run || exit 1; done

clean:
	@for e in $(EXAMPLES); do $(MAKE) -s -C $$e clean; done

.PHONY: all run clean
