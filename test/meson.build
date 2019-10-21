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

vpd_test = [ 
		'parser/parser.cpp',	
		'store/store.cpp'
	]
'''
if (get_option('kw-vpd-parser')== 'true')
	vpd_test += [
		'keyword_vpd_parser_test/kw_vpd_test.cpp'
	]
endif

if (get_option('ipz-vpd-parser')== 'true')
        vpd_test += [
                'ipz_parser/parser.cpp'
        ]
endif
'''
foreach t : vpd_test
	test(t, executable(t.underscorify(), t ,
	build_rpath: get_option('oe-sdk').enabled() ? rpath : '',

#	build_rpath: get_option('ipz_vpd_parser').enabled() ? rpath : '',
#	build_rpath: get_option('kw_vpd_parser').enabled() ? rpath : '',

	link_args: dynamic_linker,
	dependencies: [
                gtest,
                gmock,
                sdbusplus,
                phosphor_logging,
        ]),
	workdir: meson.current_source_dir())
endforeach
