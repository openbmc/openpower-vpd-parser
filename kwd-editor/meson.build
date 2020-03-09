systemd = dependency('libsystemd', version : '>= 221')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')

configuration_inc = include_directories('.', '../')

vpd_kwd_editor_SOURCES =[
                        'vpd_keyword_editor_main.cpp',
                        'vpd_keyword_editor.cpp',
                        'server.cpp',
                        'editor_impl.cpp',
                        '../impl.cpp',
                        '../parser.cpp',
                        '../utils.cpp',
                        '../vpdecc/vpdecc.c',
                        '../vpdecc/vpdecc_support.c'
                        ]

vpd_kwd_editor_dependencies =[
                             sdbusplus,
                             phosphor_logging,
                             systemd,
                             phosphor_dbus_interfaces,
                             ]

vpd_kwd_editor_exe = executable('vpd-kwd-editor',
                                 vpd_kwd_editor_SOURCES,
                                 include_directories : configuration_inc,
                                 dependencies :[
                                               vpd_kwd_editor_dependencies,
                                             ],
                                 install: true
                              )
