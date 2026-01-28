project(
    'vpd-manager',
    'c',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++23',
        'buildtype=debugoptimized',
    ],
    version: '1.0',
    meson_version: '>=1.1.1',
)

add_global_arguments(
    '-Wno-psabi',
    '-Wno-ignored-attributes',
    language: ['c', 'cpp'],
)

# Disable FORTIFY_SOURCE when compiling with no optimization
if (get_option('optimization') == '0')
    add_project_arguments('-U_FORTIFY_SOURCE', language: ['cpp', 'c'])
    message('Disabling FORTIFY_SOURCE as optimization is set to 0')
endif

# Setup googletest before we import any projects that also depend on it to make
# sure we have control over its configuration
build_tests = get_option('tests')

sdbusplus = dependency('sdbusplus', fallback: ['sdbusplus', 'sdbusplus_dep'])
phosphor_logging = dependency('phosphor-logging')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')

if build_tests.allowed()
    subdir('test')
endif

compiler = meson.get_compiler('cpp')

conf_data = configuration_data()
conf_data.set_quoted('BUSNAME', get_option('BUSNAME'))
conf_data.set_quoted('OBJPATH', get_option('OBJPATH'))
conf_data.set_quoted('IFACE', get_option('IFACE'))
conf_data.set_quoted(
    'INVENTORY_JSON_DEFAULT',
    get_option('INVENTORY_JSON_DEFAULT'),
)
conf_data.set_quoted(
    'INVENTORY_JSON_SYM_LINK',
    get_option('INVENTORY_JSON_SYM_LINK'),
)
conf_data.set_quoted(
    'JSON_ABSOLUTE_PATH_PREFIX',
    get_option('JSON_ABSOLUTE_PATH_PREFIX'),
)
conf_data.set_quoted('SYSTEM_VPD_FILE_PATH', get_option('SYSTEM_VPD_FILE_PATH'))
conf_data.set_quoted('VPD_SYMLIMK_PATH', get_option('VPD_SYMLIMK_PATH'))
conf_data.set_quoted('PIM_PATH_PREFIX', get_option('PIM_PATH_PREFIX'))
configure_file(output: 'config.h', configuration: conf_data)

services = ['service_files/vpd-manager.service']

if get_option('ibm_system').allowed()
    subdir('vpd-tool')
    subdir('wait-vpd-parser')
    subdir('active-to-passive')


    services += [
        'service_files/system-vpd.service',
        'service_files/wait-vpd-parsers.service',
        'service_files/active-to-passive.service',
    ]

    package_datadir = join_paths('share', 'vpd')
    install_subdir(
        'configuration/ibm/',
        install_mode: 'rwxr-xr-x',
        install_dir: package_datadir,
        strip_directory: true,
    )
endif

libgpiodcxx = dependency(
    'libgpiodcxx',
    default_options: ['bindings=cxx'],
    version: '<1.7.0',
)

libvpdecc_src = files('vpdecc/vpdecc.c', 'vpdecc/vpdecc_support.c')

libvpdecc = shared_library(
    'vpdecc',
    libvpdecc_src,
    version: meson.project_version(),
    install: true,
)

subdir('vpd-manager')

systemd_system_unit_dir = dependency('systemd').get_variable(
    'systemd_system_unit_dir',
)
install_data(services, install_dir: systemd_system_unit_dir)
