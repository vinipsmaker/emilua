project('my_plugin', 'cpp', default_options : ['cpp_std=c++17'])

luajit = dependency('luajit', version : '>=2.0.5')
boost = dependency('boost', version : '>=1.69')

src = [
    'src/main.cpp',
]

my_plugin_object = shared_module(
    'my_plugin',
    src,
    dependencies : [boost, luajit],
    gnu_symbol_visibility : 'hidden',
    install : true,
    install_dir : get_option('libdir') / 'emilua-0.3',
)
