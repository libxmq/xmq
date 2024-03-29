# Copyright (C) 2017-2023 Fredrik Öhrström
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

$(shell mkdir -p build)

define DQUOTE
"
endef

#" make editor quote matching happy.

HAS_GIT=$(shell git rev-parse --is-inside-work-tree >/dev/null 2>&1 ; echo $$?)

ifneq ($(HAS_GIT), 0)
# We have no git used manually set version number.
VERSION:=1.0.0
COMMIT_HASH:=
DEBVERSION:=1.0.0
else
# We have git, extract information from it.
    SUPRE=
	SUPOST=

	ifneq ($(SUDO_USER),)
        # Git has a security check to prevent the wrong user from running inside the git repository.
        # When we run "sudo make install" this will create problems since git is running as root instead.
        # Use SUPRE/SUPOST to use su to switch back to the user for the git commands.
        SUPRE=su -c $(DQUOTE)
        SUPOST=$(DQUOTE) $(SUDO_USER)
    endif

    COMMIT_HASH?=$(shell $(SUPRE) git log --pretty=format:'%H' -n 1 $(SUPOST))
    TAG?=$(shell $(SUPRE) git describe --tags $(SUPOST))
    BRANCH?=$(shell $(SUPRE) git rev-parse --abbrev-ref HEAD $(SUPOST))
    CHANGES?=$(shell $(SUPRE) git status -s | grep -v '?? ' $(SUPOST))

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
        VERSION:=$(VERSION) with uncommitted changes
        COMMIT_HASH:=$(COMMIT_HASH) but with uncommitted changes
        DEBVERSION:=$(DEBVERSION)l
    endif

endif

$(info Building $(VERSION))

$(shell echo "$(VERSION)" > build/VERSION)

all: release

help:
	@echo "Usage: make (release|debug|asan|clean|clean-all)"
	@echo "       if you have both linux64, winapi64 and arm32 configured builds,"
	@echo "       then add linux64, winapi64 or arm32 to build only for that particular host."
	@echo "E.g.:  make debug winapi64"
	@echo "       make asan"
	@echo "       make release linux64"

BUILDDIRS:=$(dir $(realpath $(filter-out build/default/%,$(wildcard build/*/spec.mk))))
FIRSTDIR:=$(word 1,$(BUILDDIRS))

ifeq (,$(BUILDDIRS))
    ifneq (clean,$(findstring clean,$(MAKECMDGOALS)))
       $(error Run configure first!)
    endif
endif

VERBOSE?=@

ifeq (winapi64,$(findstring winapi64,$(MAKECMDGOALS)))
BUILDDIRS:=$(filter %x86_64-w64-mingw32%,$(BUILDDIRS))
endif

ifeq (linux64,$(findstring linux64,$(MAKECMDGOALS)))
BUILDDIRS:=$(filter %x86_64-pc-linux-gnu%,$(BUILDDIRS))
endif

ifeq (osx64,$(findstring osx64,$(MAKECMDGOALS)))
BUILDDIRS:=$(filter %x86_64-apple-darwin%,$(BUILDDIRS))
endif

ifeq (arm32,$(findstring arm32,$(MAKECMDGOALS)))
BUILDDIRS:=$(filter %arm-unknown-linux-gnueabihf%,$(BUILDDIRS))
endif

release:
	@echo Building release for $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x release ; done

debug:
	@echo Building debug for $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x debug ; done

asan:
	@echo Building asan for $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x asan ; done

lcov:
	@echo Generating code coverage $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x debug lcov ; done

dist:
	@rm -f dist/xmq.c dist/xmq.h
	@$(MAKE) --no-print-directory -C $(FIRSTDIR) release $(shell pwd)/dist/xmq.c $(shell pwd)/dist/xmq.h
	@(cd dist; make example; make examplecc)

.PHONY: dist

test: test_release
testd: test_debug
testa: test_asan

test_release:
	@echo "Running release tests"
	@for x in $(BUILDDIRS); do echo; if [ ! -f $$x/release/testinternals ]; then echo "Run make first. $$x/release/testinternals not found."; exit 1; fi ; $$x/release/testinternals ; done
	@for x in $(BUILDDIRS); do echo;  if [ ! -f $$x/release/testinternals ]; then echo "Run make first. $$x/release/testinternals not found."; exit 1; fi ; ./tests/test.sh $$x/release $$x/release/test_output ; done

test_debug:
	@echo "Running debug tests"
	@for x in $(BUILDDIRS); do echo; if [ ! -f $$x/debug/testinternals ]; then echo "Run make first. $$x/debug/testinternals not found."; exit 1; fi ; $$x/debug/testinternals ; done
	@for x in $(BUILDDIRS); do echo;  if [ ! -f $$x/debug/testinternals ]; then echo "Run make first. $$x/debug/testinternals not found."; exit 1; fi ; ./tests/test.sh $$x/debug $$x/debug/test_output ; done

test_asan:
	@echo "Running asan tests"
	@for x in $(BUILDDIRS); do echo; if [ ! -f $$x/asan/testinternals ]; then echo "Run make first. $$x/asan/testinternals not found."; exit 1; fi ; $$x/asan/testinternals ; done
	@for x in $(BUILDDIRS); do echo;  if [ ! -f $$x/asan/testinternals ]; then echo "Run make first. $$x/asan/testinternals not found."; exit 1; fi ; ./tests/test.sh $$x/asan $$x/asan/test_output ; done

clean:
	@echo "Removing release, debug, asan, gtkdoc build dirs."
	@for x in $(BUILDDIRS); do echo; rm -rf $$x/release $$x/debug $$x/asan $$x/generated_autocomplete.h; done
	@rm -rf build/gtkdoc
	@rm -rf xmq_browsing*

clean-all:
	@echo "Removing build directory containing configuration and artifacts."
	$(VERBOSE)rm -rf build

DESTDIR?=/usr/local
install:
	install -Dm 755 -s build/x86_64-pc-linux-gnu/release/xmq $(DESTDIR)/bin/xmq
	install -Dm 755 scripts/xmq-less $(DESTDIR)/bin/xmq-less
	install -Dm 755 scripts/xmq-diff $(DESTDIR)/bin/xmq-diff
	install -Dm 755 scripts/xmq-meld $(DESTDIR)/bin/xmq-meld
	install -Dm 644 doc/xmq.1 $(DESTDIR)/man/man1/xmq.1
	install -Dm 644 scripts/autocompletion_for_xmq.sh /etc/bash_completion.d/xmq

uninstall:
	rm -f $(DESTDIR)/bin/xmq
	rm -f $(DESTDIR)/bin/xmq-less
	rm -f $(DESTDIR)/bin/xmq-diff
	rm -f $(DESTDIR)/bin/xmq-meld
	rm -f $(DESTDIR)/man/man1/xmq.1
	rm -f /etc/bash_completion.d/xmq

linux64:

arm32:

winapi64:

PACKAGE:=libxmq
PACKAGE_BUGREPORT:=
PACKAGE_NAME:=libxmq_name
PACKAGE_STRING:=libxmq_string
PACKAGE_TARNAME:=libxmq_tar
PACKAGE_URL:=https://libxmq.org/releases/libxmq.tgz
PACKAGE_VERSION:=0.9

build/gtkdocentities.ent:
	@echo '<!ENTITY package "$(PACKAGE)">' > $@
	@echo '<!ENTITY package_bugreport "$(PACKAGE_BUGREPORT)">' >> $@
	@echo '<!ENTITY package_name "$(PACKAGE_NAME)">' >> $@
	@echo '<!ENTITY package_string "$(PACKAGE_STRING)">' >> $@
	@echo '<!ENTITY package_tarname "$(PACKAGE_TARNAME)">' >> $@
	@echo '<!ENTITY package_url "$(PACKAGE_URL)">' >> $@
	@echo '<!ENTITY package_version "$(PACKAGE_VERSION)">' >> $@
	echo Created $@

gtkdoc: build/gtkdoc

build/gtkdoc: build/gtkdocentities.ent
	rm -rf build/gtkdoc
	mkdir -p build/gtkdoc
	mkdir -p build/gtkdoc/html
	cp scripts/libxmq-docs.xml build/gtkdoc
	(cd build/gtkdoc; gtkdoc-scan --module=libxmq --source-dir ../../src/main/c/)
	(cd build/gtkdoc; gtkdoc-mkdb --module libxmq --sgml-mode --source-dir ../../src/main/c --source-suffixes h  --ignore-files "xmq.c testinternals.c xmq-cli.c")
	cp build/gtkdocentities.ent build/gtkdoc/xml
	(cd build/gtkdoc/html; gtkdoc-mkhtml libxmq ../libxmq-docs.xml)
	(cd build/gtkdoc; gtkdoc-fixxref --module=libxmq --module-dir=html --html-dir=html)

.PHONY: all release debug asan test test_release test_debug clean clean-all help linux64 winapi64 arm32 gtkdoc build/gtkdoc

pom.xml: pom.xmq
	xmq pom.xmq to-xml > pom.xml

mvn:
	mvn package

.PHONY: web
web: build/web/index.html

WEBXMQ:=build/default/release/xmq

.PHONY: build/web/index.html
build/web/index.html:
	@./scripts/build_web.sh
