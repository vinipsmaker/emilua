# https://github.com/mesonbuild/meson/issues/2320

conf = configuration_data()
conf.set10('ENABLE_COLOR', not get_option('disable_color'))
conf.set10('ENABLE_HTTP', get_option('enable_http'))
configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : conf,
    install_dir : get_option('includedir') / 'emilua',
)
