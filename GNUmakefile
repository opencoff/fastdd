
# Makefile for fastdd
#
# We don't use autocrap for building this. All OS specific
# customization is split into separate files that are implemented
# carefully for each OS. e.g., blksize_darwin.c vs blksize_linux.c
#
# We keep build artifacts in a target specific directory $(objdir).
# We use a different suffix for debug vs. release builds (release
# builds have full optimization and no debug symbols).
#

os := $(shell uname)

Linux_DEFS = -D_GNU_SOURCE=1 -D_LARGEFILE64_SOURCE=1
Darwin_DEFS =

CC = gcc
CFLAGS = -Wall -Wuninitialized -g \
         -I ./portable/inc/$(os) -I./portable/inc $(DEFS) $($(os)_DEFS)
LD = $(CC)
LDFLAGS = -g

MKGETOPT = mkgetopt.py

ifeq ($(DEBUG),)
    CFLAGS += -O3
	O = rel
else
	O = dbg
endif

VPATH = . ./portable/src ./portable/src/posix

# List $(os) specific obj files here. Some files (e.g.,
# darwin_sem.o) come from portable/src/posix
Linux_objs   = blksize_linux.o   copy_linux.o
Darwin_objs  = blksize_darwin.o  copy_posix.o darwin_sem.o
OpenBSD_objs = blksize_openbsd.o copy_posix.o

# List $(os) specific libs here
Linux_LIBS = -lncurses
Darwin_LIBS =
OpenBSD_LIBS = -lpthread

LIBS += $($(os)_LIBS)

# These libobjs come from portable/src
libobjs = error.o getopt_long.o strsplit.o strcopy.o strtrim.o \
	  strtosize.o humanize.o
objs = opts.o args.o utils.o $($(os)_objs) $(libobjs)
libs = utils.a
deps = $(objs:.o=.d)
bins = fastdd disksize

ifeq ($($(os)_objs),)
$(error "No support for $(os)!" )
endif

#
# Don't modify below this
#
.SUFFIXES: .c .h .in


objdir = $(os)-$(O)

# hack to make the objdir
__ := $(shell mkdir -p $(objdir))

target-objs = $(addprefix $(objdir)/, $(objs))
target-deps = $(addprefix $(objdir)/, $(deps))
target-bins = $(addprefix $(objdir)/, $(bins))
target-libs = $(addprefix $(objdir)/, $(libs))

all: $(target-bins)

$(objdir)/fastdd: $(objdir)/fastdd.o $(objdir)/utils.a
	$(LD) $(LDFLAGS) -o $@ $(objdir)/fastdd.o $(target-libs) $(LIBS)

$(objdir)/disksize: $(objdir)/disksize.o $(objdir)/utils.a
	$(LD) $(LDFLAGS) -o $@ $(objdir)/disksize.o $(target-libs) $(LIBS)

$(objdir)/utils.a: $(target-objs)
	$(AR) rv $@ $?

$(objdir)/fastdd.o: opts.c opts.h

opts.c opts.h: opts.in
	$(MKGETOPT) $<

.PHONY: clean realclean install

test check: $(target-bins)
	./basic-tests.sh && ./tests.sh tests.in

install:
ifeq ($(DESTDIR),)
	$(error Please use DESTDIR= on the make commandline)
endif
	install -C -S $(objdir)/fastdd $(DESTDIR)/bin


clean:
	-rm -f $(target-objs) $(target-libs) $(target-bins)

realclean:
	-rm -rf $(objdir)

$(objdir)/%.o: %.c
	$(CC) -MMD $(_MP) -MT '$@ $(@:.o=.d)' -MF "$(@:.o=.d)" $(CFLAGS) -c -o $@ $<

%.i: %.c
	$(CC) -MMD $(_MP) -MT '$@ $(@:.o=.d)' -MF "$(@:.o=.d)" $(CFLAGS) -E -o $@ $<

ifneq ($(findstring clean, $(MAKECMDGOALS)),clean)
-include $(target-deps)
endif
