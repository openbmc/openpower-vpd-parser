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
REQ_SCRIPT_FILES ?= ${abs_srcdir}/writefru.yaml
EXTRA_DIST = \
	$(REQ_SCRIPT) \
	$(REQ_SCRIPT_FILES)
writefru.hpp: ${REQ_SCRIPT}
	$(AM_V_at)${REQ_SCRIPT}

bin_PROGRAMS = openpower-read-vpd
openpower_read_vpd_LDADD = libopvpdparser.la
openpower_read_vpd_SOURCES = \
	app.cpp \
	args.cpp

libopvpdparserdir = ${libdir}
libopvpdparser_LTLIBRARIES = libopvpdparser.la
libopvpdparser_la_SOURCES = \
	impl.cpp \
	parser.cpp \
	write.cpp
libopvpdparser_la_LDFLAGS = -version-info 1:0:0 -shared
