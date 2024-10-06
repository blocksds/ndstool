# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023

# Source code paths
# -----------------

SOURCEDIRS	:= source
INCLUDEDIRS	:= source

# Try to find the SDK version if it wasn't already provided e.g. by the parent Makefile
SDK_VERSION	?= $(shell git describe --tags --exact-match --dirty 2>/dev/null)

# Fallback to commit hash
ifeq ($(SDK_VERSION),)
    # --exclude to prevent any older tags from being displayed
    VERSION_ID	:= "commit $(shell git describe --always --dirty --exclude '*' 2>/dev/null)"
else
    VERSION_ID	:= "BlocksDS $(SDK_VERSION)"
endif

# Defines passed to all files
# ---------------------------

DEFINES		:= -DVERSION_ID=\"$(VERSION_ID)\"

# Libraries
# ---------

# Some hack to compile on macOS, which needs libiconv explicitly defined
UNAME		:= $(shell uname -s)
ifeq ($(UNAME),Darwin)
LIBS		:= -liconv
else
LIBS		:=
endif
LIBDIRS		:=

# Build artifacts
# ---------------

NAME		:= ndstool
BUILDDIR	:= build
ELF		:= $(NAME)

# Tools
# -----

STRIP		:= -s
BINMODE		:= 755

CC		:= gcc
CXX		:= g++
CP		:= cp
MKDIR		:= mkdir
RM		:= rm -rf
MAKE		:= make
INSTALL		:= install

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Source files
# ------------

SOURCES_C	:= $(shell find -L $(SOURCEDIRS) -name "*.c")
SOURCES_CPP	:= $(shell find -L $(SOURCEDIRS) -name "*.cpp")

# Compiler and linker flags
# -------------------------

WARNFLAGS_C	:= -Wall -Wextra -Wpedantic -Wstrict-prototypes

WARNFLAGS_CXX	:= -Wall -Wextra -Wno-unused-result -Wno-class-memaccess \
		   -Wno-stringop-truncation

ifeq ($(SOURCES_CPP),)
    LD	:= $(CC)
else
    LD	:= $(CXX)
endif

INCLUDEFLAGS	:= $(foreach path,$(INCLUDEDIRS),-I$(path)) \
		   $(foreach path,$(LIBDIRS),-I$(path)/include)

LIBDIRSFLAGS	:= $(foreach path,$(LIBDIRS),-L$(path)/lib)

CFLAGS		+= -std=gnu11 $(WARNFLAGS_C) $(DEFINES) $(INCLUDEFLAGS) -O3

CXXFLAGS	+= -std=gnu++14 $(WARNFLAGS_CXX) $(DEFINES) $(INCLUDEFLAGS) -O3

LDFLAGS		:= $(LIBDIRSFLAGS) $(LIBS)

# Intermediate build files
# ------------------------

OBJS		:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_C))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_CPP)))

DEPS		:= $(OBJS:.o=.d)

# Targets
# -------

.PHONY: all clean install

all: $(ELF)

$(ELF): $(OBJS)
	@echo "  LD      $@"
	$(V)$(LD) -o $@ $(OBJS) $(LDFLAGS)

clean:
	@echo "  CLEAN  "
	$(V)$(RM) $(ELF) $(BUILDDIR)

INSTALLDIR	?= /opt/blocksds/core/tools/ndstool
INSTALLDIR_ABS	:= $(abspath $(INSTALLDIR))

install: all
	@echo "  INSTALL $(INSTALLDIR_ABS)"
	@test $(INSTALLDIR_ABS)
	$(V)$(RM) $(INSTALLDIR_ABS)
	$(V)$(INSTALL) -d $(INSTALLDIR_ABS)
	$(V)$(INSTALL) $(STRIP) -m $(BINMODE) $(NAME) $(INSTALLDIR_ABS)
	$(V)$(CP) ./COPYING* $(INSTALLDIR_ABS)

# Rules
# -----

$(BUILDDIR)/%.c.o : %.c
	@echo "  CC      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.cpp.o : %.cpp
	@echo "  CXX     $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# Include dependency files if they exist
# --------------------------------------

-include $(DEPS)
