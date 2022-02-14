systemd = dependency('libsystemd', version: '>= 221')
sdeventplus = dependency('sdeventplus')

configuration_inc = include_directories('.', '../', '../vpd-parser/')

vpd_manager_SOURCES =['manager_main.cpp',
                      'manager.cpp',
                      'error.cpp',
                      'editor_impl.cpp',
                      'reader_impl.cpp',
                      'gpioMonitor.cpp',
                      'bios_handler.cpp',
                      '../impl.cpp',
                      '../vpd-parser/ipz_parser.cpp',
                      '../ibm_vpd_utils.cpp',
                      '../common_utility.cpp',
                      '../vpd-parser//keyword_vpd_parser.cpp',
                      '../vpd-parser/memory_vpd_parser.cpp',
                      '../vpd-parser/parser_factory.cpp'
                     ]

vpd_manager_dependencies =[phosphor_logging,
                           systemd,
                           libgpiodcxx,
                           sdeventplus,
                          ]

vpd_manager_exe = executable(
                 'vpd-manager',
                 vpd_manager_SOURCES,
                 include_directories : configuration_inc,
                 dependencies : [
                                vpd_manager_dependencies,
                                ],
                 link_with : libvpdecc,
                 install : true,
                 cpp_args : '-DIPZ_PARSER'
                )
