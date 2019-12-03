if get_option('oe-sdk').enabled()
  # Setup OE SYSROOT
  OECORE_TARGET_SYSROOT = run_command('sh', '-c', 'echo $OECORE_TARGET_SYSROOT').stdout().strip()
  if OECORE_TARGET_SYSROOT == ''
      error('Unable to get $OECORE_TARGET_SYSROOT, check your environment.')
  endif
  message('OE_SYSROOT: ' + OECORE_TARGET_SYSROOT)
  rpath = ':'.join([OECORE_TARGET_SYSROOT + '/lib', OECORE_TARGET_SYSROOT + '/usr/lib'])
  ld_so = run_command('sh', '-c', 'find ' + OECORE_TARGET_SYSROOT + '/lib/ld-*.so | sort -r -n | head -n1').stdout().strip()
  dynamic_linker = ['-Wl,-dynamic-linker,' + ld_so]
else
  dynamic_linker = []
endif

gmock = dependency('gmock', disabler: true, required: build_tests)
gtest = dependency('gtest', main: true, disabler: true, required: build_tests)

application_src = ['../impl.cpp']

test('store_test', executable('store_test',
        ['store/store.cpp', application_src],
build_rpath: get_option('oe-sdk').enabled() ? rpath : '',

link_args: dynamic_linker,
dependencies: [
            gtest,
            gmock,
            sdbusplus,
            phosphor_logging,
    ],
include_directories: '..'
),
workdir: meson.current_source_dir())

vpd_test = ['ipz_parser/parser.cpp',
                'keyword_vpd_parser_test/kw_vpd_test.cpp'
    ]
application_src += ['../keyword_vpd_parser.cpp',
  '../vpdecc/vpdecc.c',
  '../vpdecc/vpdecc_support.c'
    ]
foreach t : vpd_test
    test(t, executable(t.underscorify(),
            [t, application_src],
    build_rpath: get_option('oe-sdk').enabled() ? rpath : '',

    link_args: dynamic_linker,
    cpp_args: '-DIPZ_PARSER',
    c_args: ['-Wno-unused-parameter',
      '-Wno-unused-variable'],
    dependencies: [
                gtest,
                gmock,
                sdbusplus,
                phosphor_logging,
        ],
    include_directories: '..'
    ),
    workdir: meson.current_source_dir())
endforeach
