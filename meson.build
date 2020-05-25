project('emilua', 'cpp', default_options : ['cpp_std=c++17'])

conf = configuration_data()
conf.set10('ENABLE_COLOR', not get_option('disable_color'))
configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : conf,
)

luajit = dependency('luajit', version : '>=2.0.5')
boost = dependency(
    'boost',
    version : '>=1.69',
    modules : ['thread'],
    static : true
)
find_program('xxd')
bytecode_gen = generator(
    find_program(
        'luajit',
        dirs: [luajit.get_pkgconfig_variable('prefix') + '/bin']
    ),
    output: '@BASENAME@.cpp',
    arguments: ['--', '@INPUT@', '@OUTPUT@']
)

if get_option('disable_color')
    curses = dependency('', required : false)
else
    curses = dependency('curses', required : true)
endif

incdir = include_directories([
    '3rd/CLI11/include',
    '3rd/fmt/include',
    'include'
])

src = [
    '3rd/fmt/src/format.cc',

    'src/fiber.cpp',
    'src/timer.cpp',
    'src/state.cpp',
    'src/core.cpp',
    'src/main.cpp'
]

bytecode_src = bytecode_gen.process(
    'bytecode/sleep_for.lua',
    'bytecode/fiber.lua',
)

executable(
    'emilua',
    src,
    bytecode_src,
    cpp_pch : 'pch/pchheader.hpp',
    dependencies : [boost, luajit, curses],
    include_directories : incdir
)
