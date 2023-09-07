project(
     'openpower-vpd-parser',
     'c',
     'cpp',
     default_options: [
       'warning_level=3',
       'werror=true',
       'cpp_std=c++23',
       'buildtype=debugoptimized'
     ],
     version: '1.0',
     meson_version: '>=1.1.1',
)

add_global_arguments('-Wno-psabi', language : ['c', 'cpp'])

# Disable FORTIFY_SOURCE when compiling with no optimization
if(get_option('optimization') == '0')
  add_project_arguments('-U_FORTIFY_SOURCE',language:['cpp','c'])
  message('Disabling FORTIFY_SOURCE as optimization is set to 0')
endif

# Setup googletest before we import any projects that also depend on it to make
# sure we have control over its configuration
build_tests = get_option('tests')
if not build_tests.disabled()
    gtest_dep = dependency('gtest', main: true, disabler: true, required: false)
    gmock_dep = dependency('gmock', disabler: true, required: false)
    if not gtest_dep.found() or not gmock_dep.found()
        cmake = import('cmake')
        gtest_opts = cmake.subproject_options()
        gtest_opts.set_override_option('warning_level', '1')
        gtest_opts.set_override_option('werror', 'false')
        gtest_proj = cmake.subproject('googletest',
                                      options: gtest_opts,
                                      required: false)
        if gtest_proj.found()
            gtest_dep = declare_dependency(
                dependencies: [
                    dependency('threads'),
                    gtest_proj.dependency('gtest'),
                    gtest_proj.dependency('gtest_main'),
                ]
            )
            gmock_dep = gtest_proj.dependency('gmock')
        else
            assert(not get_option('tests').enabled(),
                   'Googletest is required if tests are enabled')
        endif
    endif
endif

phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces',
    default_options: [ 'data_com_ibm=true', 'data_org_open_power=true' ],
    fallback: ['phosphor-dbus-interfaces', 'phosphor_dbus_interfaces_dep'])

phosphor_logging = dependency('phosphor-logging',
    default_options: [ 'openpower-pel-extension=enabled' ],
    fallback: ['phosphor-logging', 'phosphor_logging_dep'])

sdbusplus = dependency('sdbusplus', fallback: [ 'sdbusplus', 'sdbusplus_dep' ])

libvpdecc_src = files(
    'vpdecc/vpdecc.c',
    'vpdecc/vpdecc_support.c'
)

libvpdecc = shared_library(
    'vpdecc',
    libvpdecc_src,
    version: meson.project_version(),
    install: true,
)

if not build_tests.disabled()
    subdir('test')
endif

compiler = meson.get_compiler('cpp')
python = find_program('python3', required:true)

compiler.has_header('CLI/CLI.hpp')
compiler.has_header('nlohmann/json.hpp')

configure_file(output: 'config.h',
                       configuration :{
                       'INVENTORY_JSON_DEFAULT': '"'+get_option('INVENTORY_JSON_DEFAULT')+'"',
                       'VPD_FILES_PATH': '"'+get_option('VPD_FILES_PATH')+'"',
                       'INVENTORY_PATH': '"'+get_option('INVENTORY_PATH')+'"',
                       'IPZ_INTERFACE': '"'+get_option('IPZ_INTERFACE')+'"',
                       'INVENTORY_MANAGER_SERVICE': '"'+get_option('INVENTORY_MANAGER_SERVICE')+'"',
                       'BUSNAME' : '"' + get_option('BUSNAME') + '"',
                       'OBJPATH' : '"' + get_option('OBJPATH') + '"',
                       'IFACE' : '"' + get_option('IFACE') + '"',
                       'OBJECT_MAPPER_SERVICE' : '"'+get_option('OBJECT_MAPPER_SERVICE')+'"',
                       'OBJECT_MAPPER_OBJECT' : '"'+get_option('OBJECT_MAPPER_OBJECT')+'"',
                       'POWER_SUPPLY_TYPE_INTERFACE' : '"'+get_option('POWER_SUPPLY_TYPE_INTERFACE')+'"',
                       'INVENTORY_MANAGER_CACHE' : '"'+get_option('INVENTORY_MANAGER_CACHE')+'"',
                       'INVENTORY_JSON_SYM_LINK': '"'+get_option('INVENTORY_JSON_SYM_LINK')+'"',
                       'INVENTORY_JSON_2U': '"'+get_option('INVENTORY_JSON_2U')+'"',
                       'INVENTORY_JSON_4U': '"'+get_option('INVENTORY_JSON_4U')+'"',
                       'INVENTORY_JSON_EVEREST': '"'+get_option('INVENTORY_JSON_EVEREST')+'"',
                       'DBUS_PROP_JSON': '"'+get_option('DBUS_PROP_JSON')+'"',
                       'SYSTEM_JSON' : '"'+get_option('SYSTEM_JSON')+'"',
                       'BAD_VPD_DIR': '"'+get_option('BAD_VPD_DIR')+'"',
                       'FAN_INTERFACE': '"'+get_option('FAN_INTERFACE')+'"'
                       }
  )

common_SOURCES =['common_utility.cpp',
'vpd-parser/parser_factory.cpp',
                 'vpd-parser/memory_vpd_parser.cpp',
                 'vpd-parser/isdimm_vpd_parser.cpp',
                 'vpd-parser/keyword_vpd_parser.cpp',
                 'vpd-parser/ipz_parser.cpp', 'impl.cpp', 'ibm_vpd_utils.cpp',
]

if get_option('ibm-parser').enabled()
        libgpiodcxx = dependency(
            'libgpiodcxx',
            default_options: ['bindings=cxx'],
        )
        ibm_read_vpd_SOURCES = ['ibm_vpd_app.cpp',
                                'vpd-manager/editor_impl.cpp',
                               ]+common_SOURCES

        vpd_tool_INCLUDE = include_directories('vpd-parser/', 'vpd-manager')

        ibm_vpd_exe = executable(
                                'ibm-read-vpd',
                                ibm_read_vpd_SOURCES,
                                dependencies: [
                                        sdbusplus,
                                        phosphor_logging,
                                        libgpiodcxx,
                                ],
                                link_with : libvpdecc,
                                include_directories : vpd_tool_INCLUDE,
                                install: true,
                                cpp_args : '-DIPZ_PARSER'
                            )

        vpd_tool_SOURCES = ['vpd_tool.cpp',
                            'vpd_tool_impl.cpp',
                            'vpd-manager/editor_impl.cpp',
                           ]+common_SOURCES


        vpd_tool_exe = executable(
                                 'vpd-tool',
                                 vpd_tool_SOURCES,
                                 dependencies: [
                                   sdbusplus,
                                   phosphor_logging,
                                   libgpiodcxx
                                   ],
                                 link_with : libvpdecc,
                                 install: true,
                                 include_directories : vpd_tool_INCLUDE,
                                 cpp_args : '-DIPZ_PARSER'
                                 )
if get_option('vpd-manager').enabled()
    subdir('vpd-manager')
endif

systemd_system_unit_dir = dependency('systemd').get_variable(
        'systemdsystemunitdir')

udev_dir = dependency('udev').get_variable(
        'udev_dir')

install_data(
    'ibm_vpd/system-vpd.service',
    install_dir: systemd_system_unit_dir)

install_data(
    'ibm_vpd/ibm-vpd-parser@.service',
    install_dir: systemd_system_unit_dir)

install_data(
    'ibm_vpd/com.ibm.VPD.Manager.service',
    install_dir: systemd_system_unit_dir)

install_data(
    'ibm_vpd/wait-vpd-parsers.service',
    install_dir: systemd_system_unit_dir)

install_data(
    'ibm_vpd/wait-vpd-parsers.sh',
    install_mode: 'rwxr-xr-x',
    install_dir: get_option('bindir'))

install_data(
    ['ibm_vpd/70-ibm-vpd-parser.rules'],
    install_dir: udev_dir / 'rules.d')

package_datadir = join_paths('share', 'vpd')
install_subdir('ibm_vpd/fru/', install_dir: package_datadir, strip_directory: true)

else
        FRUGEN = '$srcdir/extra-properties.py -e' + get_option('FRU_YAML')
        PROPGEN = '$srcdir/extra-properties.py -e' + get_option('PROP_YAML')

        src_dir = meson.project_source_root()
        FRU_GEN_SCRIPT = src_dir + '/writefru.py'
        FRU_GEN_SCRIPT_FILES = src_dir + '/writefru.yaml'

        PROP_GEN_SCRIPT = src_dir + '/extra-properties.py'
        PROP_GEN_SCRIPT_FILES = src_dir + '/extra-properties-example.yaml'

        writefru_hpp = custom_target('writefru.hpp',
                                     command:[python,
                                              FRU_GEN_SCRIPT,
                                              '-i',
                                              get_option('FRU_YAML')
                                     ],
                                     depend_files :['writefru.mako.hpp',
                                                    'writefru.py',
                                                    get_option('FRU_YAML')
                                     ],
                                     output:'writefru.hpp'
        )

        extra_properties_gen_hpp = custom_target(
                                        'extra-properties-gen.hpp',
                                        command:[
                                                python,
                                                PROP_GEN_SCRIPT,
                                                '-e',
                                                get_option('PROP_YAML')
                                        ],
                                        depend_files : ['extra-properties.mako.hpp',
                                                        'extra-properties.py',
                                                        get_option('PROP_YAML')
                                        ],
                                        output:'extra-properties-gen.hpp'
        )

        openpower_read_vpd_SOURCES = ['app.cpp',
                                      'args.cpp',
                                      'impl.cpp',
                                      'vpd-parser/ipz_parser.cpp',
                                      'write.cpp',
                                      'common_utility.cpp',
                                      writefru_hpp,
                                      extra_properties_gen_hpp
        ]

        openpower_read_vpd_exe= executable(
                'openpower-read-vpd',
                openpower_read_vpd_SOURCES,
                dependencies: [
                        sdbusplus,
                        phosphor_logging,
                ],
                include_directories : 'vpd-parser/',
                install: true,
        )
endif
