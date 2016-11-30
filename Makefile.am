# export these headers
include_HEADERS = \
	defines.hpp \
	store.hpp \
	parser.hpp \
	types.hpp \
	write.hpp \
	writefru.hpp

# Build these headers, don't export in a distro
nodist_include_HEADERS = \
	impl.hpp \
	args.hpp

# Be sure to build writefru.hpp before compiling
BUILT_SOURCES = writefru.hpp
CLEANFILES = writefru.hpp
REQ_SCRIPT ?= ${abs_srcdir}/writefru.py
writefru.hpp: ${REQ_SCRIPT}
	$(AM_V_at)${abs_srcdir}/writefru.py

bin_PROGRAMS = phosphor-read-op-vpd
phosphor_read_op_vpd_LDADD = libopvpdparser.la
phosphor_read_op_vpd_SOURCES = \
	app.cpp \
	args.cpp

libopvpdparserdir = ${libdir}
libopvpdparser_LTLIBRARIES = libopvpdparser.la
libopvpdparser_la_SOURCES = \
	impl.cpp \
	parser.cpp \
	write.cpp
libopvpdparser_la_LDFLAGS = -version-info 1:0:0 -shared

SUBDIRS = test
