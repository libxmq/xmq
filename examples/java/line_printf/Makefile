# Build the example against the locally built xmq-0.1.4.jar.
# Run make xmqj in the root of the xmq source tree first.

XMQ_ROOT ?= ../../..
JAR ?= $(XMQ_ROOT)/build/xmq-0.1.4.jar

PROG = LinePrintf

$(PROG).class: $(PROG).java
	javac -Xlint:all -cp $(JAR) $<

run: $(PROG).class
	java -cp $(JAR):. $(PROG)

clean:
	rm -f *.class

.PHONY: run clean
