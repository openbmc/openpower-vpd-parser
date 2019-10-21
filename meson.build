project(
     'openpower-vpd-parser',
     'cpp',
     default_options: [
	'cpp_std=c++17'
     ],
     version: '1.0'
)

build_tests = get_option('tests')

sdbusplus = dependency('sdbusplus')
phosphor_logging = dependency('phosphor-logging')

compiler = meson.get_compiler('cpp')
compiler.has_header('CLI/CLI.hpp')
python_dep = dependency('python', version: '2.7')
python = find_program('python', required:true)

conf = configuration_data()

if(get_option('kw-vpd-parser'))
	conf.set('KW_VPD', true)
endif

if(get_option('ipz-vpd-parser'))
	conf.set('IPZ_VPD', true)
endif

configure_file(output: 'config.h', configuration: conf)  

op_vpd_src = ['app.cpp',
        'args.cpp',
        'impl.cpp',
        'parser.cpp',
        'write.cpp',
        'utils.cpp']
op_vpd_exe = executable(
	'openpower_vpd_exe',
        op_vpd_src,
      	dependencies: [
		sdbusplus,
		phosphor_logging,
	],
	install: true,
)

kw_vpd_src = [ 'keyword_vpd_main.cpp',
        'keyword_vpd_parser.cpp']

kw_vpd_exe = executable(
	'keyword_vpd_exe',
	kw_vpd_src,
	install: true,
)
'''
ipz_vpd_src = ['ipz_app.cpp',
	'parser.cpp',
	'impl.cpp',
	'write.cpp',
	'utils.cpp']
ipz_vpd_exe = executable(
	'ipz_vpd_exe',
	ipz_vpd_src,
	dependencies: [
                sdbusplus,
                phosphor_logging,
        ],
	install: true,
)'''
subdir('test')

