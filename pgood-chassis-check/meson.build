compiler = meson.get_compiler('cpp')
libgpiodcxx = dependency(
    'libgpiodcxx',
    default_options: ['bindings=cxx'],
    version: '<1.7.0',
)

phosphor_logging = dependency('phosphor-logging')

dependency_list = [
    libgpiodcxx,
    phosphor_logging,
    sdbusplus,
    phosphor_dbus_interfaces,
]

sources = ['src/pgood-chassis-check.cpp']

pgood_chasis_check_exe = executable(
    'pgood-chassis-check',
    sources,
    include_directories: ['include/'],
    dependencies: dependency_list,
    install: true,
    cpp_args: parser_build_arguments,
)
