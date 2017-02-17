AM_CPPFLAGS = -I$(top_srcdir)

TESTS = $(check_PROGRAMS)

check_PROGRAMS = store_test
store_test_SOURCES = store/store.cpp

noinst_PROGRAMS = parser_test
parser_test_SOURCES = \
	parser/parser.cpp \
	../impl.cpp \
	../parser.cpp \
	../write.cpp \
        ../utils.cpp
parser_test_LDFLAGS = $(SDBUSPLUS_LIBS) $(PHOSPHOR_LOGGING_LIBS)
parser_test_CXXFLAGS = $(SDBUSPLUS_CFLAGS) $(PHOSPHOR_LOGGING_CFLAGS)
