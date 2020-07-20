configuration_inc = include_directories('.', '../', '../vpd-parser/')

vpd_manager_SOURCES =['manager_main.cpp',
                      'manager.cpp',
                      'server.cpp',
                      'error.cpp',
                      'editor_impl.cpp',
                      'reader_impl.cpp',
                      '../impl.cpp',
		      '../vpdecc/vpdecc.c',
		      '../vpdecc/vpdecc_support.c',
                      '../vpd-parser/ipz_parser.cpp',
                      '../utils.cpp',
                      '../vpd-parser//keyword_vpd_parser.cpp',
                      '../vpd-parser/memory_vpd_parser.cpp',
                      '../vpd-parser/parser_factory.cpp',
                     ]

vpd_manager_exe = executable(
                 'vpd-manager',
                 vpd_manager_SOURCES, 
                 include_directories : configuration_inc,
                 dependencies : [dependecy_list
                                ],
                 install : true,
		 cpp_args : '-DIPZ_PARSER'
                )
