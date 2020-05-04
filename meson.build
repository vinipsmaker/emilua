project('emilua', 'cpp', default_options : ['cpp_std=c++17'])

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

incdir = include_directories([
    '3rd/CLI11/include',
    'include'
])

src = [
    'src/timer.cpp',
    'src/state.cpp',
    'src/core.cpp',
    'src/main.cpp'
]

bytecode_src = bytecode_gen.process(
    'bytecode/sleep_for.lua',
)

executable(
    'emilua',
    src,
    bytecode_src,
    cpp_pch : 'pch/pchheader.hpp',
    dependencies : [boost, luajit],
    include_directories : incdir
)
