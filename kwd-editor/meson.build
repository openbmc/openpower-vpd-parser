systemd = dependency('libsystemd', version : '>= 221')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')

compiler.has_header('CLI/CLI.hpp')
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
                        'VPD_KeywordEditor_main.cpp',
                        'VPDKeywordEditor.cpp',
                        'server.cpp',
                        'editor.cpp',
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
