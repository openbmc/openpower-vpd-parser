AC_PREREQ([2.69])
AC_INIT([openpower-vpd-parser], [1.0], [https://github.com/openbmc/openpower-vpd-parser/issues])

AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])
AC_LANG([C++])

# Checks for programs
AC_PROG_CXX
AM_PROG_AR
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PATH_PYTHON([2.7],
    [AC_SUBST([PYTHON], [echo "$PYTHON"])],
    [AC_MSG_ERROR([Could not find python-2.7 installed...python-2.7 is required])])

PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus])
PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging])


# Suppress the --with-libtool-sysroot error
LT_INIT

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_17([noext])
AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

# Test cases require SDK so only build if we're told to (and SDK is available)
AC_ARG_ENABLE([oe-sdk],
    AS_HELP_STRING([--enable-oe-sdk], [Link testcases absolutely against OE SDK so they can be ran within it.])
)
AC_ARG_VAR(OECORE_TARGET_SYSROOT,
    [Path to the OE SDK SYSROOT])
AS_IF([test "x$enable_oe_sdk" == "xyes"],
    AS_IF([test "x$OECORE_TARGET_SYSROOT" == "x"],
          AC_MSG_ERROR([OECORE_TARGET_SYSROOT must be set with --enable-oe-sdk])
    )
    AC_MSG_NOTICE([Enabling OE-SDK at $OECORE_TARGET_SYSROOT])
    [
        testcase_flags="-Wl,-rpath,\${OECORE_TARGET_SYSROOT}/lib"
        testcase_flags="${testcase_flags} -Wl,-rpath,\${OECORE_TARGET_SYSROOT}/usr/lib"
        testcase_flags="${testcase_flags} -Wl,-dynamic-linker,`find \${OECORE_TARGET_SYSROOT}/lib/ld-*.so | sort -r -n | head -n1`"
    ]
    AC_SUBST([OESDK_TESTCASE_FLAGS], [$testcase_flags])
)

AS_IF([test "x$FRU_YAML" == "x"], [FRU_YAML="writefru.yaml"])
FRUGEN="$PYTHON $srcdir/writefru.py -i $FRU_YAML"
AC_SUBST(FRUGEN)
AS_IF([test "x$PROP_YAML" == "x"], [PROP_YAML="extra-properties-example.yaml"])
PROPGEN="$PYTHON $srcdir/extra-properties.py -e $PROP_YAML"
AC_SUBST(PROPGEN)

# Setup option for IBM VPD formats.
AC_ARG_ENABLE([ibm-parser],
              AS_HELP_STRING([--enable-ibm-parser ],
                             [Enable IBM IPZ and Keyword format VPD parser support.
            ]))

AM_CONDITIONAL([IBM_PARSER], [test "x$enable_ibm_parser" = "xyes"])
AS_IF([test "x$enable_ibm_parser" == "xyes"], [
# Check necessary header files
    AC_CHECK_HEADER(
        [CLI/CLI.hpp],
        [],
        [AC_MSG_ERROR([Could not find CLI11 CLI.hpp])]
    )
    AC_CHECK_HEADER(
        [nlohmann/json.hpp],
        [],
        [AC_MSG_ERROR([Could not find nlohmann/json.hpp. Require nlohmann/json package])]
    )
    AX_APPEND_COMPILE_FLAGS([-DIPZ_PARSER], [CXXFLAGS])
    AC_DEFINE(INVENTORY_JSON, "/usr/share/vpd/vpd_inventory.json", [JSON file that defines inventory blueprint])
            ])

# Check/set gtest specific functions.
AX_PTHREAD([GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"],[GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"])
AC_SUBST(GTEST_CPPFLAGS)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT
