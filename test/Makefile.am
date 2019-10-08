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

check_PROGRAMS += kw_vpd_test
kw_vpd_test_SOURCES = \
        keyword_vpd_parser_test/kw_vpd_test.cpp \
	../keyword_vpd_parser.cpp  

test_cppflags = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS)  \
	-pthread  \
	$(PTHREAD_CFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
        -g

test_ldflags = \
	-lgtest_main \
	-lgtest \
	-pthread  \
	$(PTHREAD_LIBS) \
	$(SDBUSPLUS_LIBS)

kw_vpd_test_CPPFLAGS = $(test_cppflags)
kw_vpd_test_LDFLAGS = $(test_ldflags)
