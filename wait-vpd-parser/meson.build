compiler = meson.get_compiler('cpp')
if compiler.has_header('CLI/CLI.hpp')
    CLI_dep = declare_dependency()
else
    CLI_dep = dependency('CLI11')
endif

sdbusplus = dependency('sdbusplus', fallback: ['sdbusplus', 'sdbusplus_dep'])

libgpiodcxx = dependency(
    'libgpiodcxx',
    default_options: ['bindings=cxx'],
    version: '<1.7.0',
)

dependency_list = [CLI_dep, sdbusplus, libgpiodcxx]

parser_build_arguments = []
if get_option('ibm_system').allowed()
    parser_build_arguments += ['-DIBM_SYSTEM']
endif

sources = [
    'src/wait_vpd_parser.cpp',
    '../vpd-manager/src/logger.cpp',
    'src/prime_inventory.cpp',
    'src/inventory_backup_handler.cpp',
    'src/collection_orchestrator.cpp',
]

wait_vpd_parser_exe = executable(
    'wait-vpd-parser',
    sources,
    include_directories: ['include/', '../', '../vpd-manager/include'],
    dependencies: dependency_list,
    install: true,
    cpp_args: parser_build_arguments,
)
