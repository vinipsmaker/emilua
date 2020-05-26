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

if get_option('enable_tests')
    # GNU Coreutils binaries are not specified explicitly, but expected too
    bash = find_program('bash')
    awk = find_program('awk')
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

emilua_bin = executable(
    'emilua',
    src,
    bytecode_src,
    cpp_pch : 'pch/pchheader.hpp',
    dependencies : [boost, luajit, curses],
    include_directories : incdir
)

if get_option('enable_tests')
    tests = {
        'fiber' : [
            'detach1',
            'detach2',
            'detach3',
            'detach4',
            'detach5',
            'detach6',
            'join1',
            'join2',
            'join3',
            'join4',
            'join5',
            'join6',
            'yield',
            'local_storage',
        ],
        'programs' : [
            'sleepsort',
        ]
    }

    foreach suite, t : tests
        foreach t : t
            test(t, bash, suite : suite,
                 args : [
                     meson.current_source_dir() + '/test/run_test.sh',
                     meson.current_source_dir() + '/test/run_test.awk',
                     meson.current_source_dir() + '/test/' + t,
                 ],
                 env : [
                     'EMILUA_BIN=' + emilua_bin.full_path(),
                     'AWK_BIN=' + awk.path(),
                 ])
        endforeach
    endforeach
endif
