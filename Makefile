# Copyright (C) 2017-2026 Fredrik Öhrström
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
VERSION:=$(shell cat dist/VERSION)
COMMIT_HASH:=
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
    TAG?=$(shell $(SUPRE) git describe --tags 2>/dev/null $(SUPOST))
    BRANCH?=$(shell $(SUPRE) git rev-parse --abbrev-ref HEAD $(SUPOST))
    CHANGES?=$(shell $(SUPRE) git status -s | grep -v '?? ' $(SUPOST))

    ifeq ($(COMMIT),$(TAG_COMMIT))
        # Exactly on the tagged commit. The version is the tag!
        VERSION:=$(TAG)
    else
        VERSION:=$(TAG)++
    endif

    ifneq ($(strip $(CHANGES)),)
        # There are changes, signify that with a +changes
        VERSION:=$(VERSION) with uncommitted changes
        COMMIT_HASH:=$(COMMIT_HASH) but with uncommitted changes
    endif

endif

ifeq (RC,$(findstring RC,$(VERSION)))
    # RC found we are making a relase.
    DIST_VERSION:=$(VERSION)
else
    DIST_VERSION:=$(shell echo "$(VERSION)" | sed 's/-.*/-modified/')
endif

$(info Building $(VERSION) (dist version $(DIST_VERSION)))

$(shell echo "$(VERSION)" > build/VERSION)

all: release

help:
	@echo "Usage: make (release|debug|asan|clean|clean-all)"
	@echo "       if you have both linux64, winapi64 and arm32 configured builds,"
	@echo "       then add linux64, winapi64 or arm32 to build only for that particular host."
	@echo "E.g.:  make debug winapi64"
	@echo "       make asan"
	@echo "       make release linux64"

BUILDDIRS:=$(dir $(realpath $(filter-out build/java/%,$(filter-out build/default/%,$(wildcard build/*/spec.mk)))))
FIRSTDIR:=$(word 1,$(BUILDDIRS))

$(info BUILDDIRS=$(BUILDDIRS))

ifeq (,$(BUILDDIRS))
    ifneq (clean,$(findstring clean,$(MAKECMDGOALS)))
        ifneq (xmqj,$(findstring xmqj,$(MAKECMDGOALS)))
            ifneq (xmqjc,$(findstring xmqjc,$(MAKECMDGOALS)))
                ifneq (testj,$(findstring testj,$(MAKECMDGOALS)))
                    $(error Run configure first!)
                endif
            endif
        endif
    endif
endif

SILENT?=nosilent

ifeq ($(SILENT),nosilent)
SILENCER:=
else
SILENCER:=| (grep ERR | true)
endif

# Only the build for one host, eg make release winapi64 (a pattern with two % does not
# match anywhere in the name, so findstring).
ifeq (winapi64,$(findstring winapi64,$(MAKECMDGOALS)))
BUILDDIRS:=$(foreach d,$(BUILDDIRS),$(if $(findstring x86_64-w64-mingw32,$(d)),$(d)))
endif

ifeq (linux64,$(findstring linux64,$(MAKECMDGOALS)))
BUILDDIRS:=$(foreach d,$(BUILDDIRS),$(if $(findstring x86_64-pc-linux-gnu,$(d)),$(d)))
endif

ifeq (osx64,$(findstring osx64,$(MAKECMDGOALS)))
BUILDDIRS:=$(foreach d,$(BUILDDIRS),$(if $(findstring x86_64-apple-darwin,$(d)),$(d)))
endif

ifeq (arm32,$(findstring arm32,$(MAKECMDGOALS)))
BUILDDIRS:=$(foreach d,$(BUILDDIRS),$(if $(findstring arm-unknown-linux-gnueabihf,$(d)),$(d)))
endif

release:
	@echo Building release for $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x release ; done
	@mkdir -p build && cp scripts/build_xmq.sh build/xmq && chmod +x build/xmq

debug:
	@echo Building debug for $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x debug ; done
	@mkdir -p build && cp scripts/build_xmq.sh build/xmq && chmod +x build/xmq

asan:
	@echo Building asan for $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x asan ; done
	@mkdir -p build && cp scripts/build_xmq.sh build/xmq && chmod +x build/xmq

# build/xmq runs the latest built xmq (release, debug or asan) with the library of the
# source tree, eg build/xmq ixml:data/csv foo.csv, see scripts/build_xmq.sh.
build/xmq: scripts/build_xmq.sh
	@mkdir -p build
	@cp scripts/build_xmq.sh build/xmq
	@chmod +x build/xmq

# AFL++ packages fetched and locally unpacked (no root, cmake or clang install needed)
AFL_DEBS:=afl++ clang clang-19 libclang1-19 libclang-common-19-dev libclang-cpp19 libclang-rt-19-dev libllvm19 llvm-19-linker-tools

# Download the AFL++ and clang .debs and unpack them under AFLplusplus/usroot
afl_prepared:
	@mkdir -p AFLplusplus/debs
	@for pkg in $(AFL_DEBS); do \
		ls AFLplusplus/debs/$${pkg}_*.deb >/dev/null 2>&1 || \
		( cd AFLplusplus/debs && apt-get download $$pkg ) || exit 1; \
	done
	@ls AFLplusplus/usroot/usr/bin/afl-cc AFLplusplus/usroot/usr/bin/clang-19 >/dev/null 2>&1 || \
	( for d in AFLplusplus/debs/*.deb; do dpkg -x $$d AFLplusplus/usroot || exit 1; done )

build_fuzz: afl_prepared
	@echo Building fuzz for $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Building $$(basename $$x) ; \
	AFL_HARDEN=1 \
	AFL_CC=$(CURDIR)/AFLplusplus/usroot/usr/bin/clang-19 \
	PATH=$(CURDIR)/AFLplusplus/usroot/usr/bin:$$PATH \
	LD_LIBRARY_PATH=$(CURDIR)/AFLplusplus/usroot/usr/lib/x86_64-linux-gnu \
	AFL_PATH=$(CURDIR)/AFLplusplus/usroot/usr/lib/afl \
	$(MAKE) --no-print-directory -C $$x CC=$(CURDIR)/AFLplusplus/usroot/usr/bin/afl-clang-fast fuzz ; done

run_fuzz_xmq:
	$(CURDIR)/AFLplusplus/usroot/usr/bin/afl-fuzz -i fuzz_testcases/xmq -o fuzz_findings_xmq -- build/x86_64-pc-linux-gnu/fuzz/xmq -

run_fuzz_yaml:
	$(CURDIR)/AFLplusplus/usroot/usr/bin/afl-fuzz -i fuzz_testcases/yaml -o fuzz_findings_yaml -- build/x86_64-pc-linux-gnu/fuzz/xmq --yaml -

# In-process libFuzzer harness (tests/fuzz/fuzz_xmq.c) over the parsers and printers,
# built with clang + AddressSanitizer + UndefinedBehaviorSanitizer from its own
# amalgamation of the parts (like dist/xmq.c, but dist/ is left untouched). The first input byte selects the content type.
#   make build_fuzz_clang
#   make run_fuzz_clang
FUZZ_CLANG_DIR:=$(FIRSTDIR)fuzz_clang
FUZZ_CLANG?=clang
FUZZ_CLANG_FLAGS?=

build_fuzz_clang:
	@mkdir -p $(FUZZ_CLANG_DIR)
	@./scripts/build_xmq_from_parts.sh $(FUZZ_CLANG_DIR) src/main/c/xmq.c dist/VERSION
	@cp $(FUZZ_CLANG_DIR)/xmq-in-progress $(FUZZ_CLANG_DIR)/xmq.c
	$(FUZZ_CLANG) -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined \
	  $(FUZZ_CLANG_FLAGS) -Isrc/main/c -o $(FUZZ_CLANG_DIR)/fuzzxmq tests/fuzz/fuzz_xmq.c $(FUZZ_CLANG_DIR)/xmq.c -lm
	@echo "Built $(FUZZ_CLANG_DIR)/fuzzxmq"

# Seeds: the first byte is the content type index (0 xmq, 1 xml, 2 html, 3 json, 4 yaml).
extract_fuzz_clang_seeds:
	@rm -rf fuzz_testcases/clang && mkdir -p fuzz_testcases/clang
	@for f in fuzz_testcases/xmq/*; do { printf '\000'; cat $$f; } > fuzz_testcases/clang/xmq_$$(basename $$f); done
	@for f in tests/libxml2/test/*.xml; do [ $$(wc -c < $$f) -le 4096 ] && { printf '\001'; cat $$f; } > fuzz_testcases/clang/xml_$$(basename $$f); done; true
	@for f in tests/libxml2/test/HTML/*.html; do [ $$(wc -c < $$f) -le 4096 ] && { printf '\002'; cat $$f; } > fuzz_testcases/clang/html_$$(basename $$f); done; true
	@for f in fuzz_testcases/json/*; do { printf '\003'; cat $$f; } > fuzz_testcases/clang/json_$$(basename $$f); done
	@for f in fuzz_testcases/yaml/*; do { printf '\004'; cat $$f; } > fuzz_testcases/clang/yaml_$$(basename $$f); done
	@echo "Extracted $$(ls fuzz_testcases/clang | wc -l) seeds into fuzz_testcases/clang"

# New inputs go into fuzz_findings_clang/corpus, crashes into fuzz_findings_clang.
run_fuzz_clang: build_fuzz_clang
	@if [ ! -d fuzz_testcases/clang ]; then $(MAKE) --no-print-directory extract_fuzz_clang_seeds; fi
	@mkdir -p fuzz_findings_clang/corpus
	$(FUZZ_CLANG_DIR)/fuzzxmq -artifact_prefix=fuzz_findings_clang/ -timeout=10 fuzz_findings_clang/corpus fuzz_testcases/clang

# Fuzz only one parser, eg make run_fuzz_clang_xmq or run_fuzz_clang_json: the harness is
# built with the content type fixed (0 xmq, 1 xml, 2 html, 3 json, 4 yaml), the inputs are
# plain files of that type (seeds fuzz_testcases/<type> or FUZZ_SEEDS_<type>), findings go into
# fuzz_findings_clang_<type>. Add the type byte (eg \003 for json) first when adding a
# finding to tests/fuzz/regressions.txt.
FUZZ_TYPE_xmq:=0
FUZZ_TYPE_xml:=1
FUZZ_TYPE_html:=2
FUZZ_TYPE_json:=3
FUZZ_TYPE_yaml:=4
# The seeds, fuzz_testcases/<type> unless the test suite has better ones.
FUZZ_SEEDS_xml:=tests/libxml2/test
FUZZ_SEEDS_html:=tests/libxml2/test/HTML

build_fuzz_clang_%:
	@$(MAKE) --no-print-directory build_fuzz_clang FUZZ_CLANG_DIR=$(FIRSTDIR)fuzz_clang_$* FUZZ_CLANG_FLAGS="$(FUZZ_CLANG_FLAGS) -DFUZZ_CONTENT_TYPE=$(FUZZ_TYPE_$*)"

run_fuzz_clang_%: build_fuzz_clang_%
	@mkdir -p fuzz_findings_clang_$*/corpus
	$(FIRSTDIR)fuzz_clang_$*/fuzzxmq -artifact_prefix=fuzz_findings_clang_$*/ -timeout=10 fuzz_findings_clang_$*/corpus $(or $(FUZZ_SEEDS_$*),fuzz_testcases/$*)

run_fuzz_yaml_until_crash:
	$(CURDIR)/scripts/fuzz_until_crash.sh \
		$(CURDIR)/AFLplusplus/usroot/usr/bin/afl-fuzz \
		fuzz_testcases/yaml \
		fuzz_findings_yaml \
		build/x86_64-pc-linux-gnu/fuzz/xmq --yaml -

lcov:
	@echo Generating code coverage $(words $(BUILDDIRS)) host\(s\).
	@for x in $(BUILDDIRS); do echo; echo Bulding $$(basename $$x) ; $(MAKE) --no-print-directory -C $$x debug lcov ; done

# Bump major number
release_major:
	@./scripts/release.sh major

# Bump minor number
release_minor:
	@./scripts/release.sh minor

# Bump patch number
release_patch:
	@./scripts/release.sh patch

# Bump release candidate number, ie a bug in the previous RC was found!
release_rc:
	@./scripts/release.sh rc

deploy:
	@./scripts/deploy.sh

dist:
	@echo "$(DIST_VERSION)" > dist/VERSION
	@rm -f dist/xmq.c dist/xmq.h
	@$(MAKE) --no-print-directory -C $(FIRSTDIR) release $(shell pwd)/dist/xmq.c $(shell pwd)/dist/xmq.h
	@(cd dist; make example; make examplecc)

java:
	@mvn -B -q package -Dgpg.skip=true

java_install:
	@mvn -B -q install -Dgpg.skip=true

java_sign:
	@mvn -B -q package

java_install_sign:
	@mvn -B -q install

.PHONY: dist

library_index:
	@./scripts/build_library_index.py

# Run the ixml community group test suite (cloned into 3rdparty/ixml if it is missing),
# the catalogs are followed and the results verified with xmq, see tests/test_ixml_standard.sh.
# Run it with another ixml processor: make test_ixml_standard PROCESSOR=coffeepot (or markup-blitz)
# Only some tests: make test_ixml_standard FILTER=ambiguous
test_ixml_standard: release
	@for x in $(BUILDDIRS); do XMQ=$${x}release/xmq FILTER="$(FILTER)" ./tests/test_ixml_standard.sh $(or $(PROCESSOR),xmq) $${x}release/test_output/ixml_standard/$(or $(PROCESSOR),xmq) || exit 1 ; done

# The same test suite run with coffeepot or markup-blitz (found in PATH or ~/bin), xmq
# still makes the plan and verifies the results. Each starts a JVM per test, so these
# take several minutes.
test_ixml_standard_coffeepot:
	@$(MAKE) --no-print-directory test_ixml_standard PROCESSOR=coffeepot

test_ixml_standard_markup_blitz:
	@$(MAKE) --no-print-directory test_ixml_standard PROCESSOR=markup-blitz

# Check that the library ixml files execute identically with xmq,coffepot and markup-blitz
# kaffepot is just a script around coffepot where -g: and -i are automatically added.
# Parse all of xmq's own preprocessed C source (the .o.source files created by the build)
# using the C grammar library/lang/c.ixml. Takes a few minutes, therefore not part of make test.
# Only test some files: make test_c_parse_myself FILTER=ixml.c or FILTER=ixml.c,xmq.c
test_c_parse_myself: release
	@for x in $(BUILDDIRS); do if [ ! -f $${x}release/xmq ]; then echo "Run make first. $${x}release/xmq not found."; exit 1; fi ; FILTER="$(FILTER)" $(if $(JOBS),JOBS="$(JOBS)") ./tests/test_c_parse_myself.sh $${x}release/xmq $${x}release/test_output/c_parse_myself $${x}release || exit 1 ; done

# The same as test_c_parse_myself, but with a C parser generated from c.ixml
# (xmqj generate-parser --c) instead of the ixml engine. Takes seconds
# instead of minutes, but cannot detect ambiguities. Needs make xmqj.
test_c_parse_myself_fast: build/xmqj release
	@for x in $(BUILDDIRS); do if [ ! -f $${x}release/xmq ]; then echo "Run make first. $${x}release/xmq not found."; exit 1; fi ; XMQJ=build/xmqj FILTER="$(FILTER)" $(if $(filter undefined,$(origin EXCLUDE)),,EXCLUDE="$(EXCLUDE)") $(if $(JOBS),JOBS="$(JOBS)") ./tests/test_c_parse_myself.sh $${x}release/xmq $${x}release/test_output/c_parse_myself_fast $${x}release || exit 1 ; done

# Parse all of xmq's own Java source (the .java files tracked by git) using the Java
# grammar library/lang/java.ixml. Only test some files: make test_java_parse_myself FILTER=Main.java
test_java_parse_myself: release
	@for x in $(BUILDDIRS); do if [ ! -f $${x}release/xmq ]; then echo "Run make first. $${x}release/xmq not found."; exit 1; fi ; FILTER="$(FILTER)" $(if $(JOBS),JOBS="$(JOBS)") ./tests/test_java_parse_myself.sh $${x}release/xmq $${x}release/test_output/java_parse_myself || exit 1 ; done

# The same as test_java_parse_myself, but with a C parser generated from java.ixml
# (xmqj generate-parser --c) instead of the ixml engine, and every tree is also cleaned
# with library/lang/java.xslq. Takes seconds, but cannot detect ambiguities. Needs make xmqj.
test_java_parse_myself_fast: build/xmqj release
	@for x in $(BUILDDIRS); do if [ ! -f $${x}release/xmq ]; then echo "Run make first. $${x}release/xmq not found."; exit 1; fi ; XMQJ=build/xmqj FILTER="$(FILTER)" $(if $(filter undefined,$(origin EXCLUDE)),,EXCLUDE="$(EXCLUDE)") $(if $(JOBS),JOBS="$(JOBS)") ./tests/test_java_parse_myself.sh $${x}release/xmq $${x}release/test_output/java_parse_myself_fast || exit 1 ; done

# Generate C parsers (xmqj generate-parser --c) for all LL(*) grammars with
# test inputs and compare their output with the ixml engine. Needs make xmqj.
test_generate_parser: build/xmqj release
	@for x in $(BUILDDIRS); do ./tests/test_generate_parser.sh $${x}release/xmq build/xmqj $${x}release/test_output/generate_parser || exit 1 ; done

# Regenerate the committed generated parsers, needs build/xmqj (make xmqj).
generate_parsers: build/xmqj
	build/xmqj generate-parser --c library/xmq-utils/xmqcli.ixml --prefix=xmqcli > src/main/c/xmq-cli-parser.h.tmp
	mv src/main/c/xmq-cli-parser.h.tmp src/main/c/xmq-cli-parser.h
	@echo "Generated src/main/c/xmq-cli-parser.h"
	build/xmqj generate-parser --java library/xmq-utils/xmqcli.ixml --package=org.libxmq.imp --class=XMQCliParser > src/main/java/org/libxmq/imp/XMQCliParser.java.tmp
	mv src/main/java/org/libxmq/imp/XMQCliParser.java.tmp src/main/java/org/libxmq/imp/XMQCliParser.java
	@echo "Generated src/main/java/org/libxmq/imp/XMQCliParser.java"

# Run the xmq examples of the test suite through library/core/xmq.ixml (ixml
# engine, generated C parser if build/xmqj exists, and the xmq.xslq round trip).
# Part of make test.
test_xmq_grammar: release
	@for x in $(BUILDDIRS); do ./tests/test_xmq_grammar.sh $${x}release/xmq build/xmqj $${x}release/test_output/xmq_grammar || exit 1 ; done

# Build and run the C and Java examples in examples and compare their output,
# the Java examples are skipped unless make xmqj has been run.
test_examples: release
	@./tests/test_examples.sh build/default/release build/test_examples

test_library:
	./tests/test_library.sh ./xmq build/test_library_output markup-blitz
	./tests/test_library.sh ./xmq build/test_library_output kaffepot

test: test_release
testd: test_debug
testa: test_asan
testv: test_valgrind
teste: test_eclipse

# Differential tests of the native xml parts against libxml2 and libxslt, needs
# 3rdparty/fetch_and_build.sh libxml2 and 3rdparty/fetch_and_build.sh libxslt
# FUZZ=count:seed sets the size of the html fuzz run (default 2000:1).
testdiff:
	@echo "Running differential tests against libxml2"
	@./tests/xml_diff/run.sh build/xml_diff

# Compare the C and the Java command lines byte for byte (to-xml, also after expand-entities)
# on all xml files of the repository and the libxml2 test suite. JOBS=n parallel jobs (default 8).
testcj: build/xmqj release
	@echo "Comparing the C and Java command lines"
	@for x in $(BUILDDIRS); do JOBS="$(JOBS)" ./tests/test_cj.sh $${x}release/xmq build/xmqj $${x}release/test_output/cj || exit 1 ; done

# Build xmq with parts excluded and run tests/test.sh on it, the tests that need an
# excluded part are skipped (xmq --features). WITHOUT is one or more of
# XMQ_PRINTING JSON YAML LIBXSLT XSD IXML HTML, eg: make test_without WITHOUT="IXML JSON"
WITHOUT_DIR:=$(CURDIR)/build/without_$(shell echo $(WITHOUT) | tr 'A-Z ' 'a-z_')
test_without:
	@if [ -z "$(WITHOUT)" ]; then echo 'Usage: make test_without WITHOUT="IXML JSON"'; exit 1; fi
	@JOBS="$(JOBS)" ./tests/build_without.sh $(WITHOUT_DIR) $(WITHOUT)
	@./tests/test.sh $(WITHOUT_DIR)/release $(WITHOUT_DIR)/release/test_output

# Build the dist style xmq.c with each of the XMQ_NO_... defines (xmq.h) that exclude a
# part, as C and C++, and check that the excluded api returns errors. JOBS=n parallel builds.
test_excluded:
	@JOBS="$(JOBS)" ./tests/test_excluded.sh build/test_excluded

# The test targets first build what they test, so that tests never run on old binaries.
test_release: release
	@echo "Running release tests"
	@for x in $(BUILDDIRS); do if [ ! -f $${x}release/testinternals ]; then echo "Run make first. $${x}release/testinternals not found."; exit 1; fi ; $${x}release/testinternals $(SILENCER) || exit 1 ; done
	@for x in $(BUILDDIRS); do if [ ! -f $${x}release/parts/testinternals ]; then echo "Run make first. $${x}release/parts/testinternals not found."; exit 1; fi ; $${x}release/parts/testinternals $(SILENCER) || exit 1 ; ./tests/test.sh $${x}release $${x}release/test_output $(FILTER) $(SILENCER) || exit 1 ; done
	@for x in $(BUILDDIRS); do $(NO_ASLR) ./tests/test_libxml2_suite.sh $${x}release/test_output/libxml2_suite || exit 1 ; done
	@for x in $(BUILDDIRS); do ./tests/test_libxslt_suite.sh $${x}release/test_output/libxslt_suite || exit 1 ; done
	@for x in $(BUILDDIRS); do $(NO_ASLR) ./tests/test_fuzz_regressions.sh $${x}release/test_output/fuzz_regressions || exit 1 ; done
	@$(MAKE) --no-print-directory test_xmq_grammar
	@$(MAKE) --no-print-directory test_examples

test_debug: debug
	@echo "Running debug tests"
	@for x in $(BUILDDIRS); do if [ ! -f $${x}debug/testinternals ]; then echo "Run make first. $${x}debug/testinternals not found."; exit 1; fi ; $${x}debug/testinternals $(SILENCER) || exit 1 ; done
	@for x in $(BUILDDIRS); do if [ ! -f $${x}debug/parts/testinternals ]; then echo "Run make first. $${x}debug/parts/testinternals not found."; exit 1; fi ; $${x}debug/parts/testinternals $(SILENCER) || exit 1 ; ./tests/test.sh $${x}debug $${x}debug/test_output $(FILTER) $(SILENCER) || exit 1 ; done

# libasan needs address randomization disabled. When it is enabled system wide, the tests
# are run with "setarch -R", which disables it for them (and their children) without root.
NO_ASLR:=$(shell if [ "$$(cat /proc/sys/kernel/randomize_va_space 2>/dev/null)" != "0" ] && setarch $$(uname -m) -R true 2>/dev/null; then echo "setarch $$(uname -m) -R"; fi)

test_asan: asan
	@echo "Running asan tests"
	@if [ "$$(cat /proc/sys/kernel/randomize_va_space)" != "0" ] && [ -z "$(NO_ASLR)" ]; then echo "Please disable address randomization for libasan to work! make disable_address_randomization"; exit 1; fi
	@for x in $(BUILDDIRS); do if [ ! -f $${x}asan/testinternals ]; then echo "Run make first. $${x}asan/testinternals not found."; exit 1; fi ; $(NO_ASLR) $${x}asan/testinternals $(SILENCER) || exit 1 ; done
	@for x in $(BUILDDIRS); do if [ ! -f $${x}asan/parts/testinternals ]; then echo "Run make first. $${x}asan/parts/testinternals not found."; exit 1; fi ; $(NO_ASLR) $${x}asan/parts/testinternals $(SILENCER) || exit 1 ; $(NO_ASLR) ./tests/test.sh $${x}asan $${x}asan/test_output $(FILTER) $(SILENCER) || exit 1 ; done

define USE_VALGRIND
valgrind -q --tool=memcheck --leak-check=full
endef

#--show-leak-kinds=all
test_valgrind: debug
	@echo "Running valgrind tests"
	@for x in $(BUILDDIRS); do if [ ! -f $${x}debug/testinternals ]; then echo "Run make first. $${x}debug/testinternals not found."; exit 1; fi ; $(USE_VALGRIND) $${x}debug/testinternals $(SILENCER) || exit 1 ; done
	@for x in $(BUILDDIRS); do if [ ! -f $${x}debug/parts/testinternals ]; then echo "Run make first. $${x}debug/parts/testinternals not found."; exit 1; fi ; $(USE_VALGRIND) $${x}debug/parts/testinternals $(SILENCER) || exit 1 ; USE_VALGRIND="$(USE_VALGRIND)" ./tests/test.sh $${x}debug $${x}debug/test_output $(FILTER) $(SILENCER) || exit 1 ; done

test_eclipse:
	@echo "Running eclipse xmq tests"
	cp build/default/spec.inc make/eclipse-cproject/Debug
	mkdir -p make/eclipse-cproject/Debug/test_output
	./tests/test.sh make/eclipse-cproject/Debug make/eclipse-cproject/Debug/test_output $(FILTER) $(SILENCER)

disable_address_randomization:
	@echo "Now running: echo 0 | sudo tee /proc/sys/kernel/randomize_va_space"
	echo 0 | sudo tee /proc/sys/kernel/randomize_va_space

enable_address_randomization:
	@echo "Now running: echo 2 | sudo tee /proc/sys/kernel/randomize_va_space"
	echo 2 | sudo tee /proc/sys/kernel/randomize_va_space

clean:
	@echo "Removing release, debug, asan, gtkdoc build dirs."
	@for x in $(BUILDDIRS); do echo; rm -rf $${x}release $${x}debug $${x}asan $${x}fuzz $${x}generated_autocomplete.h; done
	@rm -rf build/gtkdoc
	@rm -rf xmq_browsing*
	@rm -f build/xmqj
	@rm -rf build/classes

clean-all:
	@echo "Removing build directory containing configuration and artifacts."
	@rm -rf build

install_user:
	@./install.sh build/default/release "$(HOME)/.local"

uninstall_user:
	@./uninstall.sh "$(HOME)/.local"

install:
	@./install.sh build/default/release "$(DESTDIR)"

uninstall:
	@./uninstall.sh "$(DESTDIR)"

linux64:

arm32:

winapi64:

PACKAGE:=libxmq
PACKAGE_BUGREPORT:=
PACKAGE_NAME:=libxmq_name
PACKAGE_STRING:=libxmq_string
PACKAGE_TARNAME:=libxmq_tar
PACKAGE_URL:=https://libxmq.org/releases/libxmq.tgz
PACKAGE_VERSION:=$(shell cat dist/VERSION)

doxygen: build/doxygen

build/doxygen: build/Doxygenlayout.xml
	rm -rf build/doxygen
	mkdir -p build/doxygen
	doxygen doc/Doxyfile

build/Doxygenlayout.xml: doc/Doxygenlayout.xmq
	xmq doc/Doxygenlayout.xmq to-xml > build/Doxygenlayout.xml

.PHONY: build/doxygen run_fuzz_yaml

import_category:
	@if [ "$(CATEGORY)" = "" ]; then echo "Specify CATEGORY=Ll"; exit 1; fi
	curl -s https://www.fileformat.info/info/unicode/category/$(CATEGORY)/list.htm > tmp.lista
	xmq tmp.lista select //a | grep -o U+.... | sort | awk '{ print $$1","}' | tr -d '\n' | sed 's/U+/0x/g'

.PHONY: all release debug asan fuzz afl_prepared build_fuzz build_fuzz_clang test_excluded test_without extract_fuzz_clang_seeds run_fuzz_clang test testdiff testcj test_release test_debug test_c_parse_myself test_c_parse_myself_fast test_java_parse_myself test_java_parse_myself_fasttest_ixml_standard test_ixml_standard_coffeepot test_ixml_standard_markup_blitz test_generate_parser test_xmq_grammar test_examples generate_parsers clean clean-all help linux64 winapi64 arm32 gtkdoc build/gtkdoc generate_categories

pom.xml: pom.xmq jproject.settings
	@if [ "$(KEEP_POM_XML)" = "true" ]; then touch $@ ; else xmq pom.xmq to-xml > pom.xml ; xmllint --format pom.xml > tmpo; mv tmpo pom.xml; echo "Generated pom.xml" ; fi
	@. ./jproject.settings ; if ! grep -q "= $$VERSION" pom.xmq ; then echo "Version $$VERSION not found in pom.xmq"; exit 1; fi
	@. ./jproject.settings ; if ! grep -q "= $$ARTIFACTID" pom.xmq ; then echo "Artifactid $$ARTIFACTID not found in pom.xmq"; exit 1; fi
	@. ./jproject.settings ; if ! grep -q "= $$GROUPID" pom.xmq ; then echo "Groupid $$GROUPID not found in pom.xmq"; exit 1; fi


xmqj: build/xmqj

.PHONY: xmqj

# make/java/Makefile knows the java sources and only runs maven when build/xmqj is
# older than them (or pom.xml, scripts/run.sh), so always ask it.
build/xmqj: FORCE
	@(if [ ! -f build/java/spec.mk ] ; then ./make/java/configure; fi; $(MAKE) --no-print-directory -f make/java/Makefile xmqj)

FORCE:

.PHONY: FORCE

javadoc:
	@(if [ ! -f build/java/spec.mk ] ; then ./make/java/configure; fi; make --no-print-directory -f make/java/Makefile javadoc)

javadoc_imp:
	@(if [ ! -f build/java/spec.mk ] ; then ./make/java/configure; fi; make --no-print-directory -f make/java/Makefile javadoc_imp)

xmqjc: pom.xml
	@(make --no-print-directory -f make/java/Makefile xmqjc)

# Builds xmqj, and the release xmq that the java command line is compared with.
testj: build/xmqj release
	@(if [ ! -f build/java/spec.sh ] ; then ./make/java/configure; fi)
	@make --no-print-directory -f make/java/Makefile $(CURDIR)/build/project-deps/updated.timestamp
	@java -cp build/classes/ org.libxmq.imp.TestInternals
	@echo "OK: TestInternals"
# Run jar test and compiled Java programs using libxmq
	@./tests/testj.sh "" build build/test_output $(FILTER)
# The ixml trace of the Java Earley parser (xmqj --traceixml=file) and its rendering.
	@./tests/test_traceixml.sh build/xmqj build/test_output
# At most 10000 nested elements in xmq, json, xml, yaml and ixml.
	@./tests/test_max_depth_parsers.sh build/xmqj build/test_output
# Run xmqj cli binary comparing to xmq cli binary results.
#	@./tests/testj.sh build/xmqj build build/test_output $(FILTER)

testjc: xmqjc
	@java -cp build/classes/ org.libxmq.imp.TestInternals
	@echo "OK: TestInternals"
	@./tests/testj.sh build/xmqjc build build/test_output $(FILTER)

generate_categories:
	[ -e DerivedGeneralCategory.txt ] || curl -fLO https://www.unicode.org/Public/UCD/latest/ucd/extracted/DerivedGeneralCategory.txt
	@java scripts/GenUnicodeCategories.java DerivedGeneralCategory.txt src/main/c/parts/text.c

.PHONY: web
web: build/web/index.html

WEBXMQ:=build/default/release/xmq

.PHONY: build/web/index.html
build/web/index.html:
	@./scripts/build_web.sh

push_edge:
	git tag -f edge
	git push -f --tags
