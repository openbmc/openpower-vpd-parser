oem_SOURCES = []
if get_option('ibm_system').allowed()
    oem_SOURCES += ['oem-handler/ibm_handler.cpp']
endif

common_SOURCES = [
    'src/logger.cpp',
    'src/parser_factory.cpp',
    'src/ipz_parser.cpp',
    'src/keyword_vpd_parser.cpp',
    'src/ddimm_parser.cpp',
    'src/isdimm_parser.cpp',
    'src/parser.cpp',
    'src/worker.cpp',
    'src/backup_restore.cpp',
    'src/gpio_monitor.cpp',
    'src/listener.cpp',
]

vpd_manager_SOURCES = [
    'src/manager_main.cpp',
    'src/manager.cpp',
    'src/bios_handler.cpp',
] + common_SOURCES + oem_SOURCES

phosphor_interfaces_dep = dependency(
    'phosphor-dbus-interfaces',
    method: 'pkg-config',
    required: true,
)
sdbusplus_dep = dependency('sdbusplus', method: 'pkg-config', required: true)

parser_dependencies = [
    phosphor_interfaces_dep,
    libgpiodcxx,
    phosphor_logging,
    sdbusplus_dep,
]

parser_build_arguments = []
if get_option('ibm_system').allowed()
    parser_build_arguments += ['-DIBM_SYSTEM']
    parser_build_arguments += ['-DENABLE_FILE_LOGGING=true']
    vpd_manager_SOURCES += 'src/single_fab.cpp'
endif

if get_option('skip_reboot_on_fitconfig_change').allowed()
    parser_build_arguments += ['-DSKIP_REBOOT_ON_FITCONFIG_CHANGE']
endif

vpd_manager_exe = executable(
    'vpd-manager',
    vpd_manager_SOURCES,
    include_directories: ['../', 'include/', '../configuration/'],
    link_with: libvpdecc,
    dependencies: [parser_dependencies],
    install: true,
    cpp_args: parser_build_arguments,
)

vpd_parser_SOURCES = ['src/vpd_parser_main.cpp'] + common_SOURCES

vpd_parser_exe = executable(
    'vpd-parser',
    vpd_parser_SOURCES,
    include_directories: ['../', 'include/', '../configuration/'],
    link_with: libvpdecc,
    dependencies: parser_dependencies,
    install: true,
)
