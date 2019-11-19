# Build these headers, don't install them
noinst_HEADERS = \
	defines.hpp \
	store.hpp \
	parser.hpp \
	types.hpp \
	write.hpp \
	impl.hpp \
	args.hpp \
	utils.hpp \
        keyword_vpd_parser.hpp \
	ibm_vpd_type_check.hpp

if IBM_PARSER
noinst_HEADERS += \
    vpdecc/vpdecc.h\
    vpdecc/vpdecc_support.h

bin_PROGRAMS = ibm-read-vpd
ibm_read_vpd_SOURCES = \
	ibm_vpd_app.cpp \
	ibm_vpd_type_check.cpp \
	parser.cpp \
	vpdecc/vpdecc.c   \
	vpdecc/vpdecc_support.c\
	impl.cpp \
	utils.cpp \
        keyword_vpd_parser.cpp

ibm_read_vpd_LDFLAGS = $(SDBUSPLUS_LIBS) $(PHOSPHOR_LOGGING_LIBS)
ibm_read_vpd_CXXFLAGS = $(SDBUSPLUS_CFLAGS) $(PHOSPHOR_LOGGING_CFLAGS)

else
# Be sure to build these before compiling
BUILT_SOURCES = \
	writefru.hpp \
	extra-properties-gen.hpp
CLEANFILES = \
	writefru.hpp \
	extra-properties-gen.hpp

FRU_GEN_SCRIPT ?= \
	${abs_srcdir}/writefru.py
FRU_GEN_SCRIPT_FILES ?= \
	${abs_srcdir}/writefru.yaml \
	${abs_srcdir}/writefru.mako.hpp

PROP_GEN_SCRIPT ?= \
	${abs_srcdir}/extra-properties.py
PROP_GEN_SCRIPT_FILES ?= \
	${abs_srcdir}/extra-properties-example.yaml \
	${abs_srcdir}/extra-properties.mako.hpp

EXTRA_DIST = \
	$(FRU_GEN_SCRIPT) \
	$(FRU_GEN_SCRIPT_FILES) \
	$(PROP_GEN_SCRIPT) \
	$(PROP_GEN_SCRIPT_FILES)

writefru.hpp: ${FRU_GEN_SCRIPT} ${FRU_GEN_SCRIPT_FILES}
	$(AM_V_GEN)@FRUGEN@
extra-properties-gen.hpp: ${PROP_GEN_SCRIPT} ${PROP_GEN_SCRIPT_FILES}
	$(AM_V_GEN)@PROPGEN@

bin_PROGRAMS = openpower-read-vpd
openpower_read_vpd_SOURCES = \
	app.cpp \
	args.cpp \
	impl.cpp \
	parser.cpp \
	write.cpp \
	utils.cpp
openpower_read_vpd_LDFLAGS = $(SDBUSPLUS_LIBS) $(PHOSPHOR_LOGGING_LIBS)
openpower_read_vpd_CXXFLAGS = $(SDBUSPLUS_CFLAGS) $(PHOSPHOR_LOGGING_CFLAGS)
endif

SUBDIRS = test
