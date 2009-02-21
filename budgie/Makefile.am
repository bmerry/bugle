# Process with automake 1.7 or newer

# budgie is a utility problem, used for generating other files
noinst_PROGRAMS = budgie
noinst_HEADERS = tree.def c-common.def

INCLUDES = -I.. -I$(srcdir)/..
AM_YFLAGS = -d

budgie_SOURCES = \
	budgie.cpp budgie.h \
	tree.cpp tree.h \
	treeutils.cpp treeutils.h \
	tulexer.ll \
	bclexer.ll bcparser.yy
budgie_LDADD = $(REGEX_LIBS)

# Unfortunately this introduces a dependence on flex
bclexer.cc: bclexer.ll
	$(SHELL) $(YLWRAP) `test -f bclexer.ll || echo '$(srcdir)/'`bclexer.ll lex.bc_yy.c bclexer.cc -- $(LEXCOMPILE) -Pbc_yy

BUILT_SOURCES = bcparser.h
