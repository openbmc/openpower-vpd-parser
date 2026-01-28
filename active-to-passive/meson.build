compiler = meson.get_compiler('cpp')

dependency_list = []

sources = ['src/active_to_passive.cpp']

active_to_passive_exe = executable(
    'active_to_passive',
    sources,
    include_directories: ['include/', '../', '../vpd-manager/include'],
    dependencies: dependency_list,
    install: true,
)

