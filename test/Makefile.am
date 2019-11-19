AM_CPPFLAGS = -I$(top_srcdir)

TESTS = $(check_PROGRAMS)

check_PROGRAMS = store_test

store_test_SOURCES = store/store.cpp

check_PROGRAMS += ipz_parser_test

ipz_parser_test_SOURCES = \
    ipz_parser/parser.cpp \
    ../impl.cpp           \
    ../vpdecc/vpdecc.c  \
    ../vpdecc/vpdecc_support.c

test_cppflags = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS)  \
	-pthread  \
	$(PTHREAD_CFLAGS) \
	$(SDBUSPLUS_CFLAGS)

test_ldflags = \
	-lgtest_main \
	-lgtest \
	-pthread  \
	$(PTHREAD_LIBS) \
	$(SDBUSPLUS_LIBS)

ipz_parser_test_CPPFLAGS = $(test_cppflags) -DIPZ_PARSER
ipz_parser_test_LDFLAGS = $(test_ldflags)

check_PROGRAMS += kw_vpd_test
kw_vpd_test_SOURCES = \
        keyword_vpd_parser_test/kw_vpd_test.cpp \
        ../keyword_vpd_parser.cpp

kw_vpd_test_CPPFLAGS = $(test_cppflags)
kw_vpd_test_LDFLAGS = $(test_ldflags)

if !IBM_PARSER
noinst_PROGRAMS = parser_test
parser_test_SOURCES = \
	parser/parser.cpp \
    ../impl.cpp \
    ../parser.cpp \
    ../write.cpp \
    ../utils.cpp

parser_test_LDFLAGS = $(SDBUSPLUS_LIBS) $(PHOSPHOR_LOGGING_LIBS)
parser_test_CXXFLAGS = $(SDBUSPLUS_CFLAGS) $(PHOSPHOR_LOGGING_CFLAGS)

endif
