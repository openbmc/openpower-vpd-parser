compiler = meson.get_compiler('cpp')
if compiler.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency('CLI11')
endif

sdbusplus = dependency('sdbusplus', fallback: [ 'sdbusplus', 'sdbusplus_dep' ])
dependency_list = [CLI11_dep, sdbusplus]

sources = ['src/vpd_tool_main.cpp',
            'src/vpd_tool.cpp']

vpd_tool_exe = executable('vpd-tool',
                          sources,
                          include_directories : ['../', 'include/'],
                          dependencies: dependency_list,
                          install: true
                        )