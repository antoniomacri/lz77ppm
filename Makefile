
BIN = bin
LIB = lib

CFLAGS += -Wall -Wextra -std=c99
LDFLAGS += -L$(LIB)


# Takes an argument containing a list of search paths for header files (-I).
define COMPILE
	@echo ''
	@echo 'Invoking C compiler for target "$@"...'
	-@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $(addprefix -I,$1) -MMD -MP -MF"$(@:%.o=%.d)" $< -o $@
endef

# Takes an argument containing the list of object files (.o) to put into the library.
define ARCHIVE
	@echo ''
	@echo 'Invoking archiver for target "$@"...'
	-@mkdir -p $(@D)
	$(AR) -r $@ $1
endef

# Takes two arguments.
# The first argument contains the list of object files (.o) to link into the executable.
# The second argument contains the list of required libraries (-l).
define LINK
	@echo ''
	@echo 'Invoking C linker for target "$@"...'
	-@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $1 $(addprefix -l,$2)
endef

# Takes an argument containing the folder name of the module.
define GETOBJECTS
	$(eval SRCS:=$(shell find $1/src -name *.c -type f -print))$(patsubst $1/src/%,$1/obj/%, $(SRCS:.c=.o))
endef


###
### Main target
###
.PHONY: all
all: library cli


###
### liblz77ppm
###
LIBRARY := $(LIB)/liblz77ppm.a
.PHONY: library
library: $(LIBRARY)

LIBRARY_INCLUDES := liblz77ppm/api liblz77ppm/src
LIBRARY_OBJECTS := $(call GETOBJECTS,liblz77ppm)
LIBRARY_DEPS := $(LIBRARY_OBJECTS:.o=.d)

$(LIBRARY): $(LIBRARY_OBJECTS)
	$(call ARCHIVE,$(LIBRARY_OBJECTS))

liblz77ppm/obj/%.o: liblz77ppm/src/%.c
	$(call COMPILE,$(LIBRARY_INCLUDES))

-include $(LIBRARY_DEPS)

CLEAN += $(LIBRARY_OBJECTS) $(LIBRARY_DEPS) $(shell find liblz77ppm/obj/* -type d -print 2>/dev/null) liblz77ppm/obj
DISTCLEAN += $(LIBRARY) $(LIB)


###
### lz77ppm
###
CLI := $(BIN)/lz77ppm
.PHONY: cli
cli: $(CLI)

CLI_INCLUDES := liblz77ppm/api
CLI_OBJECTS := $(call GETOBJECTS,lz77ppm)
CLI_DEPS := $(CLI_OBJECTS:.o=.d)
CLI_LIBS := lz77ppm m

$(CLI): $(LIBRARY) $(CLI_OBJECTS)
	$(call LINK,$(CLI_OBJECTS),$(CLI_LIBS))

lz77ppm/obj/%.o: lz77ppm/src/%.c
	$(call COMPILE,$(CLI_INCLUDES))

-include $(CLI_DEPS)

CLEAN += $(CLI_OBJECTS) $(CLI_DEPS) lz77ppm/obj
DISTCLEAN += $(CLI) $(BIN)


###
### liblz77ppm-test
###
LIBRARYTEST := $(BIN)/liblz77ppm-test
.PHONY: test-library
test-library: $(LIBRARYTEST)

LIBRARYTEST_INCLUDES := liblz77ppm/api
LIBRARYTEST_OBJECTS := $(call GETOBJECTS,liblz77ppm-test)
LIBRARYTEST_DEPS := $(LIBRARYTEST_OBJECTS:.o=.d)
LIBRARYTEST_LIBS := lz77ppm

$(LIBRARYTEST): $(LIBRARY) $(LIBRARYTEST_OBJECTS)
	$(call LINK,$(LIBRARYTEST_OBJECTS),$(LIBRARYTEST_LIBS))

liblz77ppm-test/obj/%.o: liblz77ppm-test/src/%.c
	$(call COMPILE,$(LIBRARYTEST_INCLUDES))

-include $(LIBRARYTEST_DEPS)

TEST += $(LIBRARYTEST)
CLEAN += $(LIBRARYTEST_OBJECTS) $(LIBRARYTEST_DEPS) liblz77ppm-test/obj
DISTCLEAN += $(LIBRARYTEST) $(BIN)


###
### test
###
.PHONY: test
test: $(TEST)


###
### documentation
###
.PHONY: documentation
documentation:
	doxygen

DISTCLEAN += $(wildcard doc/html/search/*) $(wildcard doc/html/*) doc/html


###
### Clean
###
.PHONY: clean distclean
clean:
	-@rm -fd $(CLEAN) $(BIN)/*.lz $(BIN)/*.dec || true
distclean: clean
	-@rm -fd $(DISTCLEAN) || true
