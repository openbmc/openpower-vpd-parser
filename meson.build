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
if get_option('ibm-parser').enabled()
	ibm_read_vpd_SOURCES = [ 'ibm_vpd_app.cpp',
        	                'ibm_vpd_type_check.cpp',
                	        'parser.cpp',
                        	'impl.cpp',
                        	'utils.cpp',
                        	'keyword_vpd_parser.cpp'
                        	]

	ibm_vpd_exe = executable(
        	'ibm-read-vpd',
        	ibm_read_vpd_SOURCES,
        	dependencies: [
                	sdbusplus,
                	phosphor_logging,
        	],
        	install: true,
	)
else

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

	openpower_read_vpd_SOURCES = ['app.cpp',
        	'args.cpp',
	        'impl.cpp',
        	'parser.cpp',
	        'write.cpp',
        	'utils.cpp',
		writefru_hpp,
		extra_properties_gen_hpp]
	openpower_read_vpd_exe= executable(
		'openpower-read-vpd',
	        openpower_read_vpd_SOURCES,
      		dependencies: [
			sdbusplus,
			phosphor_logging,
		],
		install: true,
	)
endif
subdir('test')

