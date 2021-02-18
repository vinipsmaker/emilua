project('my_plugin', 'cpp', default_options : ['cpp_std=c++17'])

emilua = dependency('emilua')
boost = dependency('boost', version : '>=1.72')

src = [
    'src/main.cpp',
]

my_plugin_object = shared_module(
    'my_plugin',
    src,
    dependencies : [emilua, boost],
    gnu_symbol_visibility : 'hidden',
    install : true,
    install_dir : emilua.get_variable(
        pkgconfig : 'pluginsdir',
        pkgconfig_define : ['prefix', get_option('prefix')],
    ),
)
