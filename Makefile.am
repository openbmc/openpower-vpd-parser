# Build these headers, don't export in a distro
noinst_HEADERS = \
	defines.hpp \
	store.hpp \
	parser.hpp \
	types.hpp \
	write.hpp \
	impl.hpp \
	args.hpp \
	endian.hpp

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
openpower_read_vpd_SOURCES = \
	app.cpp \
	args.cpp \
	impl.cpp \
	parser.cpp \
	write.cpp
