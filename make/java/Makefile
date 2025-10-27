# xmq - Copyright 2025 Fredrik Öhrström (gpl-3.0-or-later)

ifeq ($(wildcard build/java/spec.mk),)
   $(error Please run configure!)
endif

include build/java/spec.mk

$(shell mkdir -p $(PROJECT_DEPS))

VERSION_FILE:=src/main/resources/version.txt
$(shell echo $(VERSION) > $(VERSION_FILE))

AT=@
DROP_ROOT=$(subst $(GIT_ROOT)/,./,$1)

mvn: $(BUILD)/$(EXECUTABLE)

javac: $(BUILD)/javac.timestamp

# Locate the jar dependencies automatically downloaded by maven.
JARS:=$(shell find $(PROJECT_DEPS)/ -name "*.jar" | tr '\n' ':')
$(file >build/$(EXECUTABLE).sh,java -cp $(JARS):$(BUILD)/classes $(GROUPID).Main $$*)
$(shell chmod a+x build/$(EXECUTABLE).sh)

pom.xml: pom.xmq
	$(AT)if [ ! "$(XMQ)" = "" ]; then @echo "Updating pom.xml"; xmq pom.xmq to-xml > pom.xml ; fi

# Find all java sources.
SOURCES:=$(shell find src/main/java/ -type f -name "*.java")

$(BUILD)/$(EXECUTABLE): $(PROJECT_DEPS)/updated.timestamp $(SOURCES) pom.xml scripts/run.sh
	@echo Compiling using maven
	$(AT)(MAVEN_OPTS=$(MAVEN_OPTS) mvn -B -q package)
	$(AT)cat scripts/run.sh $(BUILD)/$(ARTIFACTID)-$(VERSION)-exec.jar > $@
	$(AT)chmod a+x $@
	@echo "Built executable: $(BUILD)/$(EXECUTABLE)"

$(BUILD)/javac.timestamp: $(PROJECT_DEPS)/updated.timestamp $(SOURCES) pom.xml
	@echo Compiling javac
	$(AT)javac -classpath $(JARS) -d build/classes $(SOURCES)
	$(AT)touch $@

# The mvn tree command generates lines like this:
# [INFO] \- org.jsoup:jsoup:jar:1.11.3:compile
# from this info build the path:
# ~/.m2/repository/org/jsoup/jsoup/1.11.3/jsoup-1.11.3.jar
$(PROJECT_DEPS)/updated.timestamp: pom.xml
	@rm -rf $(PROJECT_DEPS)
	@mkdir -p $(PROJECT_DEPS)
	@echo Storing java dependencies into $(PROJECT_DEPS)
	$(AT)(MAVEN_OPTS=$(MAVEN_OPTS) mvn -DoutputDirectory=$(PROJECT_DEPS) -B -q dependency:copy-dependencies)
	@touch $(PROJECT_DEPS)/updated.timestamp

dump_databases:
	pg_dump --clean --schema-only fromcomp > fromcomp.sql
	pg_dump --clean --schema-only tocomp > tocomp.sql

recreate_database:
	psql -d fromcomp -f fromcomp.sql
	psql -d tocomp -f tocomp.sql

run:
	@java -cp $(JARS):build/classes $(GROUPID).$(ARTIFACTID).Main $(ARGS)

test: build/$(EXECUTABLE) test_dbs
	$(AT)java -cp $(JARS):build/classes $(GROUPID).$(ARTIFACTID).TestInternals
	$(AT)./tests/test.sh build/$(EXECUTABLE) build/test_output $(SOURCE) $(SINK) $(FILTER)

testv: build/$(EXECUTABLE) test_dbs
	$(AT)java -cp $(JARS):build/classes $(GROUPID).$(ARTIFACTID).TestInternals
	$(AT)(VERBOSE="-v" ./tests/test.sh build/$(EXECUTABLE) build/test_output $(SOURCE) $(SINK) $(FILTER))

test_dbs:
	./tests/download_test_dbs.sh

install_user:
	@./install.sh build/default/release "$(HOME)/.local"

uninstall_user:
	@./uninstall.sh "$(HOME)/.local"

install:
	@./install.sh build/default/release "$(DESTDIR)"

uninstall:
	@./uninstall.sh "$(DESTDIR)"

clean:
	@echo -n "Removing build directory..."
	@rm -rf build
	@echo "done."

.PHONY: clean

MAKEFLAGS += --no-builtin-rules
