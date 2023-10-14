#
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

# Expecting a spec.mk to be included already!

# To build with a lot of build output, type:
# make VERBOSE=

# Expects release or debug as a target
# make release
# make debug
# make release debug

ifeq (release,$(findstring release,$(MAKECMDGOALS)))
TYPE:=release
STRIP_COMMAND:=$(STRIP)
endif

ifeq (debug,$(findstring debug,$(MAKECMDGOALS)))
TYPE:=debug
STRIP_COMMAND:=true
endif

ifeq (asan,$(findstring asan,$(MAKECMDGOALS)))
TYPE:=asan
STRIP_COMMAND:=true
endif

ifeq ($(TYPE),)
    $(error You must specify "make release" or "make debug")
endif

$(shell mkdir -p $(OUTPUT_ROOT)/$(TYPE))

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

ifeq ($(BRANCH),master)
  BRANCH:=
else
  BRANCH:=$(BRANCH)_
endif

VERSION:=$(BRANCH)$(TAG)
DEBVERSION:=$(BRANCH)$(TAG)
LOCALCHANGES:=

ifneq ($(strip $(CHANGES)),)
  # There are local un-committed changes.
  VERSION:=$(VERSION) with local changes
  COMMIT_HASH:=$(COMMIT_HASH) with local changes
  DEBVERSION:=$(DEBVERSION)l
  LOCALCHANGES:=true
endif

$(shell echo "#define VERSION \"$(VERSION)\"" > $(OUTPUT_ROOT)/$(TYPE)/version.h.tmp)
$(shell echo "#define COMMIT \"$(COMMIT_HASH)\"" >> $(OUTPUT_ROOT)/$(TYPE)/version.h.tmp)

PREV_VERSION:=$(shell cat -n $(OUTPUT_ROOT)/$(TYPE)/version.h 2> /dev/null)
CURR_VERSION:=$(shell cat -n $(OUTPUT_ROOT)/$(TYPE)/version.h.tmp 2>/dev/null)

ifneq ($(PREV_VERSION),$(CURR_VERSION))
$(shell mv $(OUTPUT_ROOT)/$(TYPE)/version.h.tmp $(OUTPUT_ROOT)/$(TYPE)/version.h)
$(info New version number generates new $(OUTPUT_ROOT)/$(TYPE)/version.h)
else
$(shell rm $(OUTPUT_ROOT)/$(TYPE)/version.h.tmp)
endif

$(info Building $(VERSION))

VERBOSE?=@

WINAPI_SOURCES:=$(filter-out %posix.c, $(wildcard $(SRC_ROOT)/src/main/c/*.c))

WINAPI_OBJS:=\
    $(patsubst %.c,%.o,$(subst $(SRC_ROOT)/src/main/c,$(OUTPUT_ROOT)/$(TYPE),$(WINAPI_SOURCES)))

WINAPI_LIBXMQ_OBJS:=\
    $(filter-out %testinternals.o,$(WINAPI_OBJS))

WINAPI_LIBS := \
$(OUTPUT_ROOT)/$(TYPE)/libgcc_s_seh-1.dll \
$(OUTPUT_ROOT)/$(TYPE)/libstdc++-6.dll \
$(OUTPUT_ROOT)/$(TYPE)/libwinpthread-1.dll

WINAPI_TESTINTERNALS_OBJS:=\
    $(filter-out %main.o,$(WINAPI_OBJS))

POSIX_SOURCES:=$(filter-out %winapi.c,$(wildcard $(SRC_ROOT)/src/main/c/*.c))

POSIX_OBJS:=$(patsubst %.c,%.o,$(subst $(SRC_ROOT)/src/main/c,$(OUTPUT_ROOT)/$(TYPE),$(POSIX_SOURCES)))

POSIX_LIBXMQ_OBJS:=\
    $(filter-out %testinternals.o,$(POSIX_OBJS))

POSIX_TESTINTERNALS_OBJS:=\
    $(filter-out %xmq-cli.o,$(POSIX_OBJS))

POSIX_LIBS :=

EXTRA_LIBS := $($(PLATFORM)_LIBS)

$(OUTPUT_ROOT)/generated_autocomplete.h: $(SRC_ROOT)/scripts/autocompletion_for_xmq.sh
	echo Generating autocomplete include.
	cat $< | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$$/"\1\\n"/g'> $@

$(OUTPUT_ROOT)/generated_filetypes.h: $(SRC_ROOT)/scripts/filetypes.inc
	echo Generating filetypes include.
	echo '#define LIST_OF_SUFFIXES \\' > $@
	$(SRC_ROOT)/scripts/generate_filetypes.sh $(SRC_ROOT)/scripts/filetypes.inc >> $@
	echo  >> $@

LIBXMQ_OBJS:=$($(PLATFORM)_LIBXMQ_OBJS)
TESTINTERNALS_OBJS:=$($(PLATFORM)_TESTINTERNALS_OBJS)

$(OUTPUT_ROOT)/$(TYPE)/xmq_genautocomp.o: $(OUTPUT_ROOT)/generated_autocomplete.h
$(OUTPUT_ROOT)/$(TYPE)/fileinfo.o: $(OUTPUT_ROOT)/generated_filetypes.h

$(OUTPUT_ROOT)/$(TYPE)/%.o: $(SRC_ROOT)/src/main/c/%.c $(OUTPUT_ROOT)/grabbed_headers.h
	@echo Compiling $(TYPE) $(CONF_MNEMONIC) $$(basename $<)
	$(VERBOSE)$(CC) -fpic -g $(CFLAGS_$(TYPE)) $(CFLAGS) -I$(OUTPUT_ROOT) -I$(BUILD_ROOT) -MMD $< -c -o $@
	$(VERBOSE)$(CC) -E $(CFLAGS_$(TYPE)) $(CFLAGS) -I$(OUTPUT_ROOT) -I$(BUILD_ROOT) -MMD $< -c > $@.source

$(OUTPUT_ROOT)/$(TYPE)/libxmq.so: $(POSIX_OBJS)
	@echo Linking libxmq.so
	$(VERBOSE)$(CC) -shared -g -o $(OUTPUT_ROOT)/$(TYPE)/libxmq.so $(OUTPUT_ROOT)/$(TYPE)/xmq.o $(LIBXML2_LIBS) $(LDFLAGSBEGIN_$(TYPE)) $(DEBUG_LDFLAGS) $(LDFLAGSEND_$(TYPE))

$(OUTPUT_ROOT)/$(TYPE)/libxmq.a: $(POSIX_OBJS)
	@echo Archiving libxmq.a
	$(VERBOSE)ar rcs $@ $^


$(OUTPUT_ROOT)/$(TYPE)/xmq: $(LIBXMQ_OBJS)
	@echo Linking $(TYPE) $(CONF_MNEMONIC) $@
	$(VERBOSE)$(CC) -o $@ -g $(LDFLAGS_$(TYPE)) $(LDFLAGS) $(LIBXMQ_OBJS)  \
                      $(LDFLAGSBEGIN_$(TYPE)) $(ZLIB_LIBS) $(LIBXML2_LIBS) $(LDFLAGSEND_$(TYPE)) -lpthread
	$(VERBOSE)cp $@ $@.g
	$(VERBOSE)$(STRIP_COMMAND) $@

$(OUTPUT_ROOT)/grabbed_headers.h: $(SRC_ROOT)/src/main/c/xmq.c
	$(VERBOSE)$(SRC_ROOT)/scripts/grab_headers_for_testinternal.sh $< > $@
	@echo "Extracted $@ from xmq.c"

$(OUTPUT_ROOT)/$(TYPE)/testinternals: $(TESTINTERNALS_OBJS)
	@echo Linking $(TYPE) $(CONF_MNEMONIC) $@
	$(VERBOSE)$(CC) -o $@ -g $(LDFLAGS_$(TYPE)) $(LDFLAGS) $(TESTINTERNALS_OBJS) \
                      $(LDFLAGSBEGIN_$(TYPE)) $(ZLIB_LIBS) $(LIBXML2_LIBS) $(LDFLAGSEND_$(TYPE)) -lpthread
	$(VERBOSE)$(STRIP_COMMAND) $@

$(OUTPUT_ROOT)/$(TYPE)/libgcc_s_seh-1.dll: /usr/lib/gcc/x86_64-w64-mingw32/5.3-win32/libgcc_s_seh-1.dll
	cp /usr/lib/gcc/x86_64-w64-mingw32/5.3-win32/libgcc_s_seh-1.dll $@

$(OUTPUT_ROOT)/$(TYPE)/libstdc++-6.dll: /usr/lib/gcc/x86_64-w64-mingw32/5.3-win32/libstdc++-6.dll
	cp /usr/lib/gcc/x86_64-w64-mingw32/5.3-win32/libstdc++-6.dll $@

$(OUTPUT_ROOT)/$(TYPE)/libwinpthread-1.dll: /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll
	cp $< $@

BINARIES:=$(OUTPUT_ROOT)/$(TYPE)/libxmq.a $(OUTPUT_ROOT)/$(TYPE)/libxmq.so $(OUTPUT_ROOT)/$(TYPE)/xmq $(OUTPUT_ROOT)/$(TYPE)/testinternals

ifeq ($(CLEAN),clean)
# Clean!
release debug asan:
	rm -f $(OUTPUT_ROOT)/$(TYPE)/*
	rm -f $(OUTPUT_ROOT)/$(TYPE)/generated_autocomplete.h
	rm -f $(OUTPUT_ROOT)/$(TYPE)/generated_filetypes.h
else
# Build!
release debug asan: $(BINARIES) $(EXTRA_LIBS)
ifeq ($(PLATFORM),winapi)
	cp $(OUTPUT_ROOT)/$(TYPE)/xmq $(OUTPUT_ROOT)/$(TYPE)/xmq.exe
        cp $(OUTPUT_ROOT)/$(TYPE)/testinternals $(OUTPUT_ROOT)/$(TYPE)/testinternals.exe
endif
endif

clean_cc:
	find . -name "*.gcov" -delete
	find . -name "*.gcda" -delete

# This generates annotated source files ending in .gcov
# inside the build_debug where non-executed source lines are marked #####
gcov:
	@if [ "$(TYPE)" != "debug" ]; then echo "You have to run \"make debug gcov\""; exit 1; fi
	$(GCOV) -o build_debug $(PROG_OBJS) $(DRIVER_OBJS)
	mv *.gcov build_debug

# --no-external
lcov:
	@if [ "$(TYPE)" != "debug" ]; then echo "You have to run \"make debug lcov\""; exit 1; fi
	lcov --directory . -c --output-file $(OUTPUT_ROOT)/$(TYPE)/lcov.info
	(cd $(OUTPUT_ROOT)/$(TYPE); genhtml lcov.info)
	xdg-open $(OUTPUT_ROOT)/$(TYPE)/c/index.html

.PHONY: release debug

# Include dependency information generated by gcc in a previous compile.
include $(wildcard $(patsubst %.o,%.d,$(POSIX_OBJS)))
