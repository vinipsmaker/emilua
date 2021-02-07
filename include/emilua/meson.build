# https://github.com/mesonbuild/meson/issues/2320

conf = configuration_data()

conf.set('VERSION_MAJOR', version_major)
conf.set('VERSION_MINOR', version_minor)
conf.set('VERSION_PATCH', version_patch)
conf.set_quoted(
    'VERSION_STRING',
    version_major.to_string() + '.' +
    version_minor.to_string() + '.' +
    version_patch.to_string() +
    get_option('version_suffix')
)
conf.set_quoted('LIBROOTDIR', get_option('prefix') / get_option('libdir'))
conf.set10('ENABLE_COLOR', not get_option('disable_color'))
conf.set10('ENABLE_HTTP', get_option('enable_http'))

configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : conf,
    install_dir : get_option('includedir') / 'emilua',
)
