# https://github.com/mesonbuild/meson/issues/2320

conf = configuration_data()

conf.set('VERSION_MAJOR', meson.project_version().split('.')[0].to_int())
conf.set('VERSION_MINOR', meson.project_version().split('.')[1].to_int())
conf.set('VERSION_PATCH', meson.project_version().split('.')[2].to_int())
conf.set_quoted(
    'VERSION_STRING',
    meson.project_version() + get_option('version_suffix')
)
conf.set_quoted('LIBROOTDIR', get_option('prefix') / get_option('libdir'))
conf.set10('ENABLE_PLUGINS', get_option('enable_plugins'))
conf.set10('ENABLE_COLOR', not get_option('disable_color'))
conf.set10('ENABLE_HTTP', get_option('enable_http'))
conf.set10(
    'ENABLE_WEBSOCKET_PERMESSAGE_DEFLATE',
    get_option('enable_websocket_permessage_deflate')
)
conf.set10('ENABLE_FILE_IO', get_option('enable_file_io'))
conf.set('THREAD_SUPPORT_LEVEL',  get_option('thread_support_level'))

conf.set10('ENABLE_LINUX_NAMESPACES', get_option('enable_linux_namespaces'))
conf.set('LINUX_NAMESPACES_MSG_SIZE',
         get_option('linux_namespaces_msg_size'))
conf.set('LINUX_NAMESPACES_MSG_MAX_MEMBERS_NUMBER',
         get_option('linux_namespaces_msg_max_members_number'))

configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : conf,
    install_dir : get_option('includedir') / 'emilua',
)
