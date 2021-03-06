#############################################################################
# Root Makefile

#############################################################################
# Derived options

# Implementation system
ifndef CONF_SYSTEM
ifeq ($(CONF_HOST),dos)
CONF_SYSTEM=$(CONF_HOST)
else
ifeq ($(CONF_HOST),windows)
CONF_SYSTEM=$(CONF_HOST)
else
CONF_SYSTEM=unix
endif
endif
endif

# Binaries names
BINARYTAG = $(CONF_HOST)-$(CONF_ARCH)
BINARYDIR = $(CONF_HOST)/$(CONF_ARCH)
BINARYBUILDDIR = $(CONF_BUILD)

#############################################################################
# CFLAGS/LDFLAGS

# Override any environment CFLAGS/LDFLAGS declaration
# - CONF_DEFS are the defines from autoconf like HAVE_LIBZ_H...
# - CONF_CFLAGS_ARCH are the architecture flags
# - CONF_CFLAGS_OPT are the optimization flags
CFLAGS = $(CONF_DEFS) $(CONF_CFLAGS_ARCH) $(CONF_CFLAGS_OPT)
LDFLAGS = $(CONF_LDFLAGS)
LIBS = $(CONF_LIBS)

# Compiling message
ifeq ($(CONF_DEBUG),yes)
MSG = "(debug)"
else
MSG =
CFLAGS += -DNDEBUG
endif

#############################################################################
# Advance

# Get the version if not using ./configure
ifeq ($(VERSION),)
VERSION=$(shell sh autover.sh)
endif

include $(srcdir)/advance/advance.mak

ifneq ($(wildcard $(srcdir)/advance/emu.mak),)
include $(srcdir)/advance/emu.mak
endif
ifneq ($(wildcard $(srcdir)/advance/lib.mak),)
include $(srcdir)/advance/lib.mak
endif
ifneq ($(wildcard $(srcdir)/advance/menu.mak),)
include $(srcdir)/advance/menu.mak
endif
ifneq ($(wildcard $(srcdir)/advance/cab.mak),)
include $(srcdir)/advance/cab.mak
endif
ifneq ($(wildcard $(srcdir)/advance/v.mak),)
include $(srcdir)/advance/v.mak
endif
ifneq ($(wildcard $(srcdir)/advance/cfg.mak),)
include $(srcdir)/advance/cfg.mak
endif
ifneq ($(wildcard $(srcdir)/advance/s.mak),)
include $(srcdir)/advance/s.mak
endif
ifneq ($(wildcard $(srcdir)/advance/k.mak),)
include $(srcdir)/advance/k.mak
endif
ifneq ($(wildcard $(srcdir)/advance/i.mak),)
include $(srcdir)/advance/i.mak
endif
ifneq ($(wildcard $(srcdir)/advance/j.mak),)
include $(srcdir)/advance/j.mak
endif
ifneq ($(wildcard $(srcdir)/advance/m.mak),)
include $(srcdir)/advance/m.mak
endif
ifneq ($(wildcard $(srcdir)/advance/line.mak),)
include $(srcdir)/advance/line.mak
endif
ifneq ($(wildcard $(srcdir)/advance/d2.mak),)
include $(srcdir)/advance/d2.mak
endif

#############################################################################
# Standard GNU targets

info:

dvi:

install-strip: install

check:

installcheck: install check

mostlyclean: distclean

maintainer-clear: distclean

clean:
	$(RM) -f -r obj

distclean: clean
	$(RM) -f config.status config.log

