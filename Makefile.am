# Build these headers, don't install them
noinst_HEADERS = \
	defines.hpp \
	store.hpp \
	parser.hpp \
	types.hpp \
	write.hpp \
	impl.hpp \
	args.hpp \
	types.hpp \
	utils.hpp

# Be sure to build writefru.hpp before compiling
BUILT_SOURCES = writefru.hpp
CLEANFILES = writefru.hpp
REQ_SCRIPT ?= ${abs_srcdir}/writefru.py
REQ_SCRIPT_FILES ?= \
	${abs_srcdir}/writefru.yaml \
	${abs_srcdir}/writefru.mako.hpp
EXTRA_DIST = \
	$(REQ_SCRIPT) \
	$(REQ_SCRIPT_FILES)
writefru.hpp: ${REQ_SCRIPT} ${REQ_SCRIPT_FILES}
	$(AM_V_GEN)@FRUGEN@

sbin_PROGRAMS = openpower-read-vpd
openpower_read_vpd_SOURCES = \
	app.cpp \
	args.cpp \
	impl.cpp \
	parser.cpp \
	write.cpp \
	utils.cpp
openpower_read_vpd_LDFLAGS = $(SDBUSPLUS_LIBS)
openpower_read_vpd_CXXFLAGS = $(SDBUSPLUS_CFLAGS)

SUBDIRS = test
