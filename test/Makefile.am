AM_CPPFLAGS = -I$(top_srcdir)

check_PROGRAMS =

TESTS = $(check_PROGRAMS)

check_PROGRAMS += store_test
store_test_SOURCES = store/store.cpp

check_PROGRAMS += parser_test
parser_test_SOURCES = \
	parser/parser.cpp \
	../impl.cpp \
	../parser.cpp \
	../write.cpp
