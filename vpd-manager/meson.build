project('vpd-kwd-editor',
        'cpp',
        'c',
        default_options : ['cpp_std=c++17'],
        version : '1.0')
sdbusplus = dependency('sdbusplus')
phosphor_logging = dependency('phosphor-logging')
systemd = dependency('libsystemd', version : '>= 221')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')

compiler = meson.get_compiler('cpp')
compiler.has_header('nlohmann/json.hpp')

configure_file( output: 'config.h',
                        configuration: {
                        'BUSNAME' : '"' + get_option('BUSNAME') + '"',
                        'OBJPATH' : '"' + get_option('OBJPATH') + '"',
                        'IFACE' : '"' + get_option('IFACE') + '"',
                        'INVENTORY_JSON' : '"' + get_option('INVENTORY_JSON') + '"'
                        }
                )

configuration_inc = include_directories('.', '../')

vpd_kwd_editor_SOURCES =[
                        'manager_main.cpp',
                        'manager.cpp',
                        'server.cpp',
                        'editor_impl.cpp',
                        '../impl.cpp',
                        '../parser.cpp',
                        '../utils.cpp',
                        '../vpdecc/vpdecc.c'
                        ]

vpd_kwd_editor_exe = executable('vpd-kwd-editor',
                                 vpd_kwd_editor_SOURCES,
                                 include_directories : configuration_inc,
                                 dependencies :[
                                                 sdbusplus,
                                                 phosphor_logging,
                                                 systemd,
                                                 phosphor_dbus_interfaces,
                                             ],
                              )
