systemd = dependency('libsystemd', version: '>= 221')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')

configuration_inc = include_directories('.', '../', '../vpd-parser/')

vpd_manager_SOURCES =['manager_main.cpp',
                      'manager.cpp',
                      'server.cpp',
                      'error.cpp',
                      'editor_impl.cpp',
                      'reader_impl.cpp',
                      '../impl.cpp',
                      '../vpd-parser/ipz_parser.cpp',
                      '../ibm_vpd_utils.cpp',
                      '../common_utility.cpp',
                      '../vpdecc/vpdecc.c',
                      '../vpdecc/vpdecc_support.c',
                      '../vpd-parser/keyword_vpd_parser.cpp',
                      '../vpd-parser/memory_vpd_parser.cpp',
                      '../vpd-parser/parser_factory.cpp'
                     ]

vpd_manager_dependencies =[sdbusplus,
                           phosphor_logging,
                           systemd,
                           phosphor_dbus_interfaces,
                          ]

vpd_manager_exe = executable(
                 'vpd-manager',
                 vpd_manager_SOURCES, 
                 include_directories : configuration_inc,
                 dependencies : [
                                vpd_manager_dependencies,
                                ],
                 install : true
                )
