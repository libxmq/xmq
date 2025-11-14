# xmq - Copyright 2025 Fredrik Öhrström (gpl-3.0-or-later)

ifeq ($(wildcard build/java/spec.mk),)
   $(error Please run configure!)
endif

include build/java/spec.mk

$(shell mkdir -p $(PROJECT_DEPS))

VERSION_FILE:=src/main/resources/version.txt
$(shell mkdir -p src/main/resources)
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
	@echo "Built package: $(BUILD)/$(ARTIFACTID)-$(VERSION).jar"

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

javadoc:
	javadoc -d build/javadoc -sourcepath src/main/java org.libxmq


MAKEFLAGS += --no-builtin-rules
