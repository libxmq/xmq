#
# Copyright (C) 2017-2024 Fredrik Öhrström
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
# make VERBOSE=1
# or make V=1

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
    $(error You must specify "make release" or "make debug" or "make asan")
endif

$(shell mkdir -p $(OUTPUT_ROOT)/$(TYPE)/parts $(SRC_ROOT)/dist)

VERSION=$(shell cat $(SRC_ROOT)/build/VERSION)
VERBOSE?=0
V?=0
AT:=@

ifeq ($(VERBOSE),1)
    AT:=
endif
ifeq ($(V),1)
    AT:=
endif

CFLAGS += -DVERSION='"$(VERSION)"'

# The SOURCES are what makes up libxmq xmq.
SOURCES:=$(wildcard $(SRC_ROOT)/src/main/c/*.c)

# The PARTS_SOURCES are what gets copied into xmq.c
# But we test the parts separately here.
PARTS_SOURCES:=$(wildcard $(SRC_ROOT)/src/main/c/parts/*.c)

WINAPI_SOURCES:=$(filter-out %posix.c, $(SOURCES))
WINAPI_PARTS_SOURCES:=$(filter-out %posix.c, $(PARTS_SOURCES))
WINAPI_OBJS:=\
    $(patsubst %.c,%.o,$(subst $(SRC_ROOT)/src/main/c,$(OUTPUT_ROOT)/$(TYPE),$(WINAPI_SOURCES))) $(patsubst %.c,%.o,$(subst $(SRC_ROOT)/src/main/c,$(OUTPUT_ROOT)/$(TYPE),$(WINAPI_PARTS_SOURCES)))
WINAPI_LIBXMQ_OBJS:=$(filter-out %xmq-cli.o, $(filter-out %parts/testinternals.o, $(filter-out %testinternals.o,$(WINAPI_OBJS))))
WINAPI_TESTINTERNALS_OBJS:=$(filter-out %xmq-cli.o,$(WINAPI_LIBXMQ_OBJS))
WINAPI_LIBS := \
$(OUTPUT_ROOT)/$(TYPE)/libgcc_s_seh-1.dll \
$(OUTPUT_ROOT)/$(TYPE)/libstdc++-6.dll \
$(OUTPUT_ROOT)/$(TYPE)/libwinpthread-1.dll \
$(OUTPUT_ROOT)/$(TYPE)/libxml2-2.dll \
$(OUTPUT_ROOT)/$(TYPE)/libxslt-1.dll
WINAPI_SUFFIX:=.exe

POSIX_SOURCES:=$(filter-out %winapi.c,$(SOURCES))
POSIX_PARTS_SOURCES:=$(filter-out %winapi.c,$(PARTS_SOURCES))
POSIX_OBJS:=$(patsubst %.c,%.o,$(subst $(SRC_ROOT)/src/main/c,$(OUTPUT_ROOT)/$(TYPE),$(POSIX_SOURCES))) $(patsubst %.c,%.o,$(subst $(SRC_ROOT)/src/main/c,$(OUTPUT_ROOT)/$(TYPE),$(POSIX_PARTS_SOURCES)))
POSIX_LIBXMQ_OBJS:=$(filter-out %xmq-cli.o,$(filter-out %parts/testinternals.o, $(filter-out %testinternals.o,$(POSIX_OBJS))))
POSIX_TESTINTERNALS_OBJS:=$(filter-out %xmq-cli.o,$(POSIX_LIBXMQ_OBJS))
POSIX_LIBS:=
POSIX_SUFFIX:=

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
SUFFIX:=$($(PLATFORM)_SUFFIX)

$(OUTPUT_ROOT)/$(TYPE)/xmq_genautocomp.o: $(OUTPUT_ROOT)/generated_autocomplete.h
$(OUTPUT_ROOT)/$(TYPE)/fileinfo.o: $(OUTPUT_ROOT)/generated_filetypes.h

$(OUTPUT_ROOT)/$(TYPE)/%.o: $(SRC_ROOT)/src/main/c/%.c $(PARTS_SOURCES) $(OUTPUT_ROOT)/update_yaep
	@echo Compiling $(TYPE) $(CONF_MNEMONIC) $$(basename $<)
	$(AT)$(CC) -fpic -g $(CFLAGS_$(TYPE)) $(CFLAGS) -I$(SRC_ROOT)/src/main/c -I$(OUTPUT_ROOT) -I$(BUILD_ROOT) -MMD $< -c -o $@
	$(AT)$(CC) -E $(CFLAGS_$(TYPE)) $(CFLAGS) -I$(SRC_ROOT)/src/main/c -I$(OUTPUT_ROOT) -I$(BUILD_ROOT) -MMD $< -c > $@.source

$(OUTPUT_ROOT)/$(TYPE)/parts/%.o: $(SRC_ROOT)/src/main/c/parts/%.c
	@echo Compiling part $(TYPE) $(CONF_MNEMONIC) $$(basename $<)
	$(AT)$(CC) -fpic -g $(CFLAGS_$(TYPE)) $(CFLAGS) -I$(SRC_ROOT)/src/main/c -I$(OUTPUT_ROOT) -I$(BUILD_ROOT) -MMD $< -c -o $@
	$(AT)$(CC) -E $(CFLAGS_$(TYPE)) $(CFLAGS) -I$(SRC_ROOT)/src/main/c -I$(OUTPUT_ROOT) -I$(BUILD_ROOT) -MMD $< -c > $@.source

ifneq ($(PLATFORM),WINAPI)
$(OUTPUT_ROOT)/$(TYPE)/libxmq.so: $(POSIX_OBJS)
	@echo "Linking libxmq.so"
	$(AT)$(CC) -shared -g -o $(OUTPUT_ROOT)/$(TYPE)/libxmq.so $(LIBXMQ_OBJS) $(LIBXML2_LIBS) $(LIBXSLT_LIBS) $(LDFLAGSBEGIN_$(TYPE)) $(DEBUG_LDFLAGS) $(LDFLAGSEND_$(TYPE))
else
$(OUTPUT_ROOT)/$(TYPE)/libxmq.so: $(WINAPI_OBJS) $(PARTS_SOURCES)
	touch $@
endif

$(OUTPUT_ROOT)/$(TYPE)/libxmq.a: $(LIBXMQ_OBJS)
	@echo Archiving libxmq.a
	$(AT)ar rcs $@ $^

ifeq ($(ENABLE_STATIC_XMQ),no)
$(OUTPUT_ROOT)/$(TYPE)/xmq: $(OUTPUT_ROOT)/$(TYPE)/xmq-cli.o $(LIBXMQ_OBJS) $(EXTRA_LIBS)
	@echo Linking $(TYPE) $(CONF_MNEMONIC) $@
	$(AT)$(CC) -o $@ -g $(LDFLAGS_$(TYPE)) $(LDFLAGS) $(OUTPUT_ROOT)/$(TYPE)/xmq-cli.o $(LIBXMQ_OBJS) \
                      $(LDFLAGSBEGIN_$(TYPE)) $(ZLIB_LIBS) $(LIBXML2_LIBS) $(LIBXSLT_LIBS) $(LDFLAGSEND_$(TYPE)) -lpthread -lm
	$(AT)cp $@ $@.g
	$(AT)$(STRIP_COMMAND) $@$(SUFFIX)
ifeq ($(PLATFORM),WINAPI)
	$(AT)mkdir -p build/windows_installer
	$(AT)cp $(OUTPUT_ROOT)/$(TYPE)/xmq.exe $(OUTPUT_ROOT)/$(TYPE)/*.dll build/windows_installer
	$(AT)cp scripts/xmq.nsis build/windows_installer
	$(AT)(cd build/windows_installer; makensis xmq.nsis)
endif
else
$(OUTPUT_ROOT)/$(TYPE)/xmq: $(OUTPUT_ROOT)/$(TYPE)/xmq-cli.o $(LIBXMQ_OBJS) $(EXTRA_LIBS)
	@echo Linking static $(TYPE) $(CONF_MNEMONIC) $@
	$(AT)$(CC) -static -o $@ $(LDFLAGS_$(TYPE)) $(LDFLAGS) $(OUTPUT_ROOT)/$(TYPE)/xmq-cli.o $(LIBXMQ_OBJS) \
                      $(LDFLAGSBEGIN_$(TYPE)) $(ZLIB_LIBS) $(LIBXML2_LIBS) $(LIBXSLT_LIBS) $(LDFLAGSEND_$(TYPE)) -lpthread -lm
ifeq ($(PLATFORM),WINAPI)
	$(AT)mkdir -p $(OUTPUT_ROOT)/windows_installer
	$(AT)cp $(OUTPUT_ROOT)/$(TYPE)/xmq.exe $(OUTPUT_ROOT)/$(TYPE)/*.dll $(OUTPUT_ROOT)/windows_installer
	$(AT)cp $(SRC_ROOT)/scripts/windows-installer-wixl.wxs $(OUTPUT_ROOT)/windows_installer/xmq-windows-$(TYPE).wxs
	$(AT)(cd $(OUTPUT_ROOT)/windows_installer; wixl -v xmq-windows-$(TYPE).wxs)
endif
endif

$(OUTPUT_ROOT)/$(TYPE)/testinternals: $(OUTPUT_ROOT)/$(TYPE)/testinternals.o $(TESTINTERNALS_OBJS)
	@echo Linking $(TYPE) $(CONF_MNEMONIC) $@
	$(AT)$(CC) -o $@ -g $(LDFLAGS_$(TYPE)) $(LDFLAGS) \
	    $(OUTPUT_ROOT)/$(TYPE)/testinternals.o $(TESTINTERNALS_OBJS) \
	    $(LDFLAGSBEGIN_$(TYPE)) $(ZLIB_LIBS) $(LIBXML2_LIBS) $(LIBXSLT_LIBS) $(LDFLAGSEND_$(TYPE)) -lpthread -lm
	$(AT)$(STRIP_COMMAND) $@$(SUFFIX)

$(OUTPUT_ROOT)/$(TYPE)/parts/testinternals: $(OUTPUT_ROOT)/$(TYPE)/parts/testinternals.o $(TESTINTERNALS_OBJS)
	@echo Linking parts $(TYPE) $(CONF_MNEMONIC) $@
	$(AT)$(CC) -o $@ -g $(LDFLAGS_$(TYPE)) $(LDFLAGS) \
	    $(OUTPUT_ROOT)/$(TYPE)/parts/testinternals.o $(TESTINTERNALS_OBJS) \
            $(LDFLAGSBEGIN_$(TYPE)) $(ZLIB_LIBS) $(LIBXML2_LIBS) $(LIBXSLT_LIBS) $(LDFLAGSEND_$(TYPE)) -lpthread -lm
	$(AT)$(STRIP_COMMAND) $@$(SUFFIX)

$(OUTPUT_ROOT)/$(TYPE)/libgcc_s_seh-1.dll:
	$(AT)cp "$$(find /usr/lib/gcc -name libgcc_s_seh-1.dll | grep -m 1 win32)" $@
	@echo "Installed $@"

$(OUTPUT_ROOT)/$(TYPE)/libstdc++-6.dll:
	$(AT)cp "$$(find /usr/lib/gcc -name libstdc++-6.dll | grep -m 1 win32)" $@
	@echo "Installed $@"

$(OUTPUT_ROOT)/$(TYPE)/libwinpthread-1.dll:
	$(AT)cp "$$(find /usr -name libwinpthread-1.dll | grep -m 1 x86_64)" $@
	@echo "Installed $@"

$(OUTPUT_ROOT)/$(TYPE)/libxml2-2.dll:
	$(AT)cp "$$(find $(SRC_ROOT)/3rdparty/libxml2-winapi -name libxml2-2.dll | grep -m 1 libxml2-2.dll)" $@
	@echo "Installed $@"

$(OUTPUT_ROOT)/$(TYPE)/libxslt-1.dll:
	$(AT)cp "$$(find $(SRC_ROOT)/3rdparty/libxslt-winapi -name libxslt-1.dll | grep -m 1 libxslt-1.dll)" $@
	@echo "Installed $@"

BINARIES:=$(OUTPUT_ROOT)/$(TYPE)/libxmq.a \
          $(OUTPUT_ROOT)/$(TYPE)/libxmq.so \
          $(OUTPUT_ROOT)/$(TYPE)/xmq \
          $(OUTPUT_ROOT)/$(TYPE)/testinternals \
          $(OUTPUT_ROOT)/$(TYPE)/parts/testinternals

$(SRC_ROOT)/dist/xmq.h: $(SRC_ROOT)/src/main/c/xmq.h
	@cp $< $@
	@echo "Copied dist/xmq.h"

$(SRC_ROOT)/dist/xmq.c: $(SRC_ROOT)/src/main/c/xmq.c $(PARTS_SOURCES) $(OUTPUT_ROOT)/update_yaep
	$(AT)$(SRC_ROOT)/scripts/build_xmq_from_parts.sh $(OUTPUT_ROOT) $< $(SRC_ROOT)/dist/VERSION
	$(AT)cp $(OUTPUT_ROOT)/xmq-in-progress $(SRC_ROOT)/dist/xmq.c
	@echo "Generated dist/xmq.c"

$(OUTPUT_ROOT)/update_yaep: $(SRC_ROOT)/src/main/c/yaep/src/yaep.c $(SRC_ROOT)/src/main/c/yaep/src/yaep.h
	@rm -f $(SRC_ROOT)/src/main/c/parts/yaep.h $(SRC_ROOT)/src/main/c/parts/yaep.c
	@(cd $(SRC_ROOT)/src/main/c/yaep ; ./build_single_yaep_source_file.sh)
	@echo Generated yaep.h yaep.c
	@touch $@

ifeq ($(CLEAN),clean)
# Clean!
release debug asan:
	rm -f $(OUTPUT_ROOT)/$(TYPE)/*
	rm -f $(OUTPUT_ROOT)/$(TYPE)/generated_autocomplete.h
	rm -f $(OUTPUT_ROOT)/$(TYPE)/generated_filetypes.h
	rm -f $(OUTPUT_ROOT)/update_yaep
else
# Build!
release debug asan: $(BINARIES) $(EXTRA_LIBS)
#ifeq ($(PLATFORM),winapi)
#	cp $(OUTPUT_ROOT)/$(TYPE)/xmq $(OUTPUT_ROOT)/$(TYPE)/xmq.exe
#	cp $(OUTPUT_ROOT)/$(TYPE)/testinternals $(OUTPUT_ROOT)/$(TYPE)/testinternals.exe
#endif
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
#include $(wildcard $(patsubst %.o,%.d,$(POSIX_OBJS)))
