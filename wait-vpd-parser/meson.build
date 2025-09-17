compiler = meson.get_compiler('cpp')
if compiler.has_header('CLI/CLI.hpp')
    CLI_dep = declare_dependency()
else
    CLI_dep = dependency('CLI11')
endif

sdbusplus = dependency('sdbusplus', fallback: ['sdbusplus', 'sdbusplus_dep'])
dependency_list = [CLI_dep, sdbusplus]

sources = ['src/wait_vpd_parser.cpp', '../vpd-manager/src/logger.cpp']

wait_vpd_parser_exe = executable(
    'wait-vpd-parser',
    sources,
    include_directories: ['../', '../vpd-manager/include'],
    dependencies: dependency_list,
    install: true,
)
