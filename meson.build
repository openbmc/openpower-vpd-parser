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

#code_added
FRUGEN = '$srcdir/extra-properties.py -e' + get_option('FRU_YAML')
PROPGEN = '$srcdir/extra-properties.py -e' + get_option('PROP_YAML')

src_dir = meson.source_root()

FRU_GEN_SCRIPT = src_dir + '/writefru.py'
FRU_GEN_SCRIPT_FILES = src_dir + '/writefru.yaml'

PROP_GEN_SCRIPT = src_dir + '/extra-properties.py'
PROP_GEN_SCRIPT_FILES = src_dir + '/extra-properties-example.yaml'

writefru_hpp = custom_target(
				'writefru.hpp',
				command:[
					python,
					FRU_GEN_SCRIPT,
					'-i',
					get_option('FRU_YAML')
					],
				depend_files : ['writefru.mako.hpp',
						'writefru.py',
						get_option('FRU_YAML')
						],
				output:'writefru.hpp'
			)
extra_properties_gen_hpp = custom_target(
				'extra-properties-gen.hpp',
				command:[
					python,
					PROP_GEN_SCRIPT,
					'-e',
					get_option('PROP_YAML')
					],
				depend_files : ['extra-properties.mako.hpp',
						'extra-properties.py',
						get_option('PROP_YAML')
						],
				output:'extra-properties-gen.hpp'
			)
#ends here
#AC_SUBST is not needed in meson file as it doesnot uses two file i.e. configure and makefile
conf = configuration_data()

if(get_option('kw-vpd-parser').enabled())
	conf.set('KW_VPD', true)
endif
'''
if(get_option('ipz-vpd-parser'))
	conf.set('IPZ_VPD', true)
endif
'''
configure_file(output: 'config.h', configuration: conf)  

op_vpd_src = ['app.cpp',
        'args.cpp',
        'impl.cpp',
        'parser.cpp',
        'write.cpp',
        'utils.cpp',
	writefru_hpp,
	extra_properties_gen_hpp]
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

