gtest_dep = dependency('gtest', main: true, disabler: true, required: false)
gmock_dep = dependency('gmock', disabler: true, required: false)
libgpiodcxx = dependency('libgpiodcxx', default_options: ['bindings=cxx'])
if not gtest_dep.found() or not gmock_dep.found()
    cmake = import('cmake')
    gtest_opts = cmake.subproject_options()
    gtest_opts.set_override_option('warning_level', '1')
    gtest_opts.set_override_option('werror', 'false')
    gtest_proj = cmake.subproject(
        'googletest',
        options: gtest_opts,
        required: false,
    )
    if gtest_proj.found()
        gtest_dep = declare_dependency(
            dependencies: [
                dependency('threads'),
                gtest_proj.dependency('gtest'),
                gtest_proj.dependency('gtest_main'),
            ],
        )
        gmock_dep = gtest_proj.dependency('gmock')
    else
        assert(
            not get_option('tests').allowed(),
            'Googletest is required if tests are enabled',
        )
    endif
endif

parser_build_arguments = []
if get_option('ipz_ecc_check').allowed()
    parser_build_arguments += ['-DIPZ_ECC_CHECK']
endif

dependency_list = [gtest_dep, gmock_dep, sdbusplus, libgpiodcxx]

configuration_inc = include_directories(
    '..',
    '../vpd-manager/include',
    '../vpdecc',
)

test_sources = [
    '../vpd-manager/src/logger.cpp',
    '../vpd-manager/src/ddimm_parser.cpp',
    '../vpd-manager/src/parser.cpp',
    '../vpd-manager/src/parser_factory.cpp',
    '../vpd-manager/src/isdimm_parser.cpp',
    '../vpd-manager/src/ipz_parser.cpp',
    '../vpd-manager/src/keyword_vpd_parser.cpp',
    '../vpdecc/vpdecc.c',
]

tests = [
    'utest_utils.cpp',
    'utest_keyword_parser.cpp',
    'utest_ddimm_parser.cpp',
    'utest_ipz_parser.cpp',
    'utest_json_utility.cpp',
]

foreach test_file : tests
    test(
        test_file,
        executable(
            test_file.underscorify(),
            test_file,
            test_sources,
            include_directories: configuration_inc,
            dependencies: dependency_list,
            cpp_args: parser_build_arguments,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
