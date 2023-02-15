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
libgpiodcxx = dependency('libgpiodcxx')
dependecy_list = [gtest, gmock, sdbusplus, phosphor_logging, phosphor_dbus_interfaces, libgpiodcxx]

configuration_inc = include_directories('..', '../vpd-manager', 'vpd-manager-test', '../vpd-parser')

vpd_test = ['store/store.cpp',
            'ipz_parser/parser.cpp',
            'keyword_vpd_parser_test/kw_vpd_test.cpp',
            'vpd-manager-test/reader_test.cpp',
            'vpd-manager-test/editor_test.cpp'
]

application_src =['../impl.cpp',
                   '../vpd-parser/ipz_parser.cpp',
                   '../ibm_vpd_utils.cpp',
                   '../common_utility.cpp',
                   '../vpd-manager/reader_impl.cpp',
                   '../vpd-parser/keyword_vpd_parser.cpp',
                   '../vpd-manager/editor_impl.cpp',
                   '../vpd-parser/parser_factory.cpp',
                   '../vpd-parser/memory_vpd_parser.cpp',
                   '../vpd-parser/isdimm_vpd_parser.cpp'
                  ]

foreach t : vpd_test
    test(t, executable(t.underscorify(),
            [t, application_src],
    build_rpath: get_option('oe-sdk').enabled() ? rpath : '',
    link_args: dynamic_linker,
    cpp_args: ['-DIPZ_PARSER', '-DManagerTest'],
    c_args: ['-Wno-unused-parameter',
      '-Wno-unused-variable'],
    dependencies: dependecy_list,
    include_directories: configuration_inc,
    link_with : libvpdecc,
    ),
    workdir: meson.current_source_dir())
endforeach
