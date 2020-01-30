project(
     'vpd-kwd-editor',
     'cpp',
     default_options: [
	'cpp_std=c++17'
     ],
     version: '1.0'
)

sdbusplus = dependency('sdbusplus')
phosphor_logging = dependency('phosphor-logging')
systemd = dependency('libsystemd', version: '>= 221')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')



compiler = meson.get_compiler('cpp')
compiler.has_header('CLI/CLI.hpp')
python_dep = dependency('python', version: '2.7')
python = find_program('python', required:true)

configure_file(output: 'config.h',
		 configuration :{
		'BUSNAME': '"'+get_option('BUSNAME')+'"',
		'OBJPATH': '"'+get_option('OBJPATH')+'"',
		'IFACE': '"'+get_option('IFACE')+'"',
		}
	)
configuration_inc = include_directories('.', '../')

vpd_kwd_editor_SOURCES = [ 'VPD_KeywordEditor_main.cpp',
				'VPDKeywordEditor.cpp',
				'server.cpp',
				]
vpd_kwd_editor_exe = executable(
				'vpd-kwd-editor',
				vpd_kwd_editor_SOURCES,
				include_directories : configuration_inc, 
				dependencies:[
					sdbusplus,
					phosphor_logging,
					systemd,
					phosphor_dbus_interfaces,
				],
				)
