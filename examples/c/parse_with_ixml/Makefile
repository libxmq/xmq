# Build the example against the locally built libxmq.so.
# Run make in the root of the xmq source tree first.

XMQ_ROOT ?= ../../..
LIBDIR ?= $(XMQ_ROOT)/build/default/release
CFLAGS ?= -g -O0 -Wall

PROG = parse_with_ixml

$(PROG): $(PROG).c
	$(CC) $(CFLAGS) -I$(XMQ_ROOT)/src/main/c -o $@ $< -L$(LIBDIR) -Wl,-rpath,$(abspath $(LIBDIR)) -lxmq

run: $(PROG)
	./$(PROG)

clean:
	rm -f $(PROG)

.PHONY: run clean
