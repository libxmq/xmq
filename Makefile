# Copyright (C) 2019-2020 Fredrik Öhrström

# To compile for Raspberry PI ARM:
# make HOST=arm
#
# To build with debug information:
# make DEBUG=true
# make DEBUG=true HOST=arm

ifeq "$(HOST)" "arm"
    CXX=arm-linux-gnueabihf-g++
    STRIP=arm-linux-gnueabihf-strip
    BUILD=build_arm
	DEBARCH=armhf
else
    CXX=g++
    STRIP=strip
#--strip-unneeded --remove-section=.comment --remove-section=.note
    BUILD=build
	DEBARCH=amd64
endif

ifeq "$(DEBUG)" "true"
    DEBUG_FLAGS=-O0 -ggdb -fsanitize=address -fno-omit-frame-pointer
    STRIP_BINARY=
    BUILD:=$(BUILD)_debug
    DEBUG_LDFLAGS=-lasan
else
    DEBUG_FLAGS=-Os
    STRIP_BINARY=$(STRIP) $(BUILD)/xmq
endif

$(shell mkdir -p $(BUILD) target)

COMMIT_HASH:=$(shell git log --pretty=format:'%H' -n 1)
TAG:=$(shell git describe --tags)
CHANGES:=$(shell git status -s | grep -v '?? ')
TAG_COMMIT_HASH:=$(shell git show-ref --tags | grep $(TAG) | cut -f 1 -d ' ')

ifeq ($(COMMIT),$(TAG_COMMIT))
  # Exactly on the tagged commit. The version is the tag!
  VERSION:=$(TAG)
  DEBVERSION:=$(TAG)
else
  VERSION:=$(TAG)++
  DEBVERSION:=$(TAG)++
endif

ifneq ($(strip $(CHANGES)),)
  # There are changes, signify that with a +changes
  VERSION:=$(VERSION) with local changes
  COMMIT_HASH:=$(COMMIT_HASH) with local changes
  DEBVERSION:=$(DEBVERSION)l
endif

$(shell echo "#define VERSION \"$(VERSION)\"" > $(BUILD)/version.h.tmp)
$(shell echo "#define COMMIT \"$(COMMIT_HASH)\"" >> $(BUILD)/version.h.tmp)

PREV_VERSION=$(shell cat -n $(BUILD)/version.h 2> /dev/null)
CURR_VERSION=$(shell cat -n $(BUILD)/version.h.tmp 2>/dev/null)
ifneq ($(PREV_VERSION),$(CURR_VERSION))
$(shell mv -f $(BUILD)/version.h.tmp $(BUILD)/version.h)
else
$(shell rm -f $(BUILD)/version.h.tmp)
endif

$(info Building $(VERSION))

CXXFLAGS := $(DEBUG_FLAGS) -fPIC -fmessage-length=0 -std=c++11 -Wall -Wno-unused-function -I$(BUILD) -I.

#	$(CXX) $(CXXFLAGS) $< -c -E > $@.src

$(BUILD)/%.o: src/main/cc/%.cc $(wildcard src/%.h)
	$(CXX) $(CXXFLAGS) $< -MMD -fPIC -c -o $@

XMQ_OBJS:=\
	$(BUILD)/cmdline.o \
	$(BUILD)/util.o \
	$(BUILD)/parse.o \
	$(BUILD)/render.o \
	$(BUILD)/xmq_implementation.o

XMQ_LIB_OBJS:=\
	$(BUILD)/parse.o \
	$(BUILD)/render.o \
	$(BUILD)/xmq_implementation.o

all: $(BUILD)/xmq $(BUILD)/libxmq.so $(BUILD)/libxmq.a $(BUILD)/testinternals testur
	@$(STRIP_BINARY)

.PHONY: dist
dist: $(BUILD)/libxmq.so $(BUILD)/libxmq.a src/main/cc/xmq.h src/main/cc/xmq_rapidxml.h
	@mkdir -p dist
	@cp $(BUILD)/libxmq.so dist
	@cp $(BUILD)/libxmq.a dist
	@cp src/main/cc/xmq.h dist
	@cp src/main/cc/xmq_rapidxml.h dist

$(BUILD)/xmq: $(XMQ_OBJS) $(BUILD)/main.o
	$(CXX) -o $(BUILD)/xmq $(XMQ_OBJS) $(BUILD)/main.o $(DEBUG_LDFLAGS)

$(BUILD)/libxmq.so: $(XMQ_LIB_OBJS)
	$(CXX) -shared -o $(BUILD)/libxmq.so $(XMQ_LIB_OBJS) $(DEBUG_LDFLAGS)

$(BUILD)/libxmq.a: $(XMQ_LIB_OBJS)
	ar rcs $@ $^

$(BUILD)/testinternals: $(XMQ_OBJS) $(BUILD)/testinternals.o
	$(CXX) -o $(BUILD)/testinternals $(XMQ_OBJS) $(BUILD)/testinternals.o $(DEBUG_LDFLAGS)

clean:
	rm -rf build/* build_arm/* build_debug/* build_arm_debug/* *~

test:
	@./build/testinternals
	@./spec/genspechtml.sh ./build/xmq

testur: dist testur.cc
	@$(CXX) $(CXXFLAGS) testur.cc -o $@ -Idist -I. -Ldist -lxmq

run_testur: testur
	LD_LIBRARY_PATH=dist ./testur

testdebug:
	@echo Test internals
	@./build_debug/testinternals
	@./test.sh build_debug

install:
	rm -f /usr/share/man/man1/wmbusmeters.1.gz
	mkdir -p /usr/share/man/man1
	gzip -c xmq.1 > /usr/share/man/man1/xmq.1.gz
	cp build/xmq /usr/local/bin
	cp scripts/xmq-less /usr/local/bin
	cp scripts/xmq-diff /usr/local/bin
	cp scripts/xmq-meld /usr/local/bin
	cp scripts/xmq-git-diff /usr/local/bin
	cp scripts/xmq-git-meld /usr/local/bin

install_emacs:
	mkdir -p ~/.emacs.d/lisp
	cp xmq-mode.el ~/.emacs.d/lisp/xmq-mode.el

# The mvn tree command generates lines like this:
# [INFO] \- org.jsoup:jsoup:jar:1.11.3:compile
# from this info build the path:
# ~/.m2/repository/org/jsoup/jsoup/1.11.3/jsoup-1.11.3.jar
project-deps/updated: pom.xml
	@rm -rf project-deps
	@mkdir -p project-deps
	@echo Storing java dependencies locally...
	@mvn dependency:resolve
	@DEPS=`mvn dependency:tree | grep INFO | grep compile | grep -oE '[^ ]+$$'` ; \
    for DEP in $$DEPS ; do \
        GROU=$$(echo $$DEP | cut -f 1 -d ':' | sed 's|\.|/|g') ; \
        ARTI=$$(echo $$DEP | cut -f 2 -d ':') ; \
        SUFF=$$(echo $$DEP | cut -f 3 -d ':') ; \
        VERS=$$(echo $$DEP | cut -f 4 -d ':') ; \
        JAR="$$GROU/$$ARTI/$$VERS/$$ARTI-$$VERS.$$SUFF" ; \
        echo Stored $$JAR ; \
        cp ~/.m2/repository/$$JAR project-deps ; \
    done
	@touch project-deps/updated

mvn:
	mvn -B compile

JAVA_SOURCES=$(wildcard src/main/java/org/ammunde/xmq/*)
JAVA_DEPS=$(shell find project-deps/ -name "*.jar" | tr '\n' ':')

javac: $(JAVA_SOURCES) project-deps/updated
	@javac -cp $(JAVA_DEPS) $(JAVA_SOURCES) -d target/classes

xmqj: $(JAVA_SOURCES) project-deps/updated
	@VM=`java -version 2>&1 | grep Runtime | grep -o "GraalVM"` ; \
	    if [ "$$VM" != "GraalVM" ]; then echo Please install Graal VM to build native image; exit 1; fi
	@echo Compiling to native image
	native-image --verbose -cp target/classes:$(JAVA_DEPS) org.ammunde.xmq.Main build/xmqj

gencompare: $(BUILD)/xmq

# Include dependency information generated by gcc in a previous compile.
include $(wildcard $(patsubst %.o,%.d,$(XMQ_OBJS)))
