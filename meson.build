project('emilua', 'cpp', default_options : ['cpp_std=c++17'])

conf = configuration_data()
conf.set10('ENABLE_COLOR', not get_option('disable_color'))
configure_file(
    input : 'meson/config.h.in',
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
    shell = find_program('bash', 'ksh')
    gawk = find_program('gawk')
endif

incdir = include_directories([
    '3rd/CLI11/include',
    '3rd/fmt/include',
    'include'
])

src = [
    '3rd/fmt/src/format.cc',

    'src/scope_cleanup.cpp',
    'src/lua_shim.cpp',
    'src/fiber.cpp',
    'src/mutex.cpp',
    'src/timer.cpp',
    'src/state.cpp',
    'src/cond.cpp',
    'src/core.cpp',
    'src/main.cpp'
]

bytecode_src = bytecode_gen.process(
    'bytecode/sleep_for.lua',
    'bytecode/lua_shim.lua',
    'bytecode/fiber.lua',
    'bytecode/scope.lua',
    'bytecode/state.lua',
    'bytecode/cond.lua',
)

emilua_bin = executable(
    'emilua',
    src,
    bytecode_src,
    cpp_pch : 'meson/pchheader.hpp',
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
            'join7',
            'join8',
            'yield',
            'local_storage',
            'forbid_suspend_setup1',
            'forbid_suspend_setup2',
            'forbid_suspend_setup3',
            'forbid_suspend_join',
            'forbid_suspend_yield',
            'forbid_suspend_sleep_for',
            'interrupt1',
            'interrupt2',
            'interrupt3',
            'interrupt5',
            'interrupt6',
            'interrupt7',
            'interrupt8',
            'interrupt9',
            'interrupt10',
            'interrupt11',
            'interrupt12',
            'interrupt13',
            'interrupt14',
            'interrupt15',
            'interrupt16',
            'interrupt17',
            'interrupt18',
            'non-portable/interrupt1',
        ],
        'sync' : [
            'mutex1',
            'mutex2',
            'mutex3',
            'mutex4',
            'cond1',
            'cond2',
            'cond3',
            'cond4',
            'cond5',
            'cond6',
            'cond7',
            'cond8',
            'non-portable/mutex1',
        ],
        'scope' : [
            'scope1',
            'scope2',
            'scope3',
            'scope4',
            'scope5',
            'scope6',
            'scope7',
            'scope8',
            'scope9',
            'scope_pcall1',
            'scope_xpcall1',
            'scope_nested1',
            'scope_nested2',
        ],
        'lua_shim' : [
            'coroutine_running1',
            'coroutine_running2',
            'coroutine_yield1',
            'coroutine_yield2',
            'coroutine_yield3',
            'coroutine_resume1',
            'coroutine_resume2',
            'coroutine_resume3',
            'coroutine_resume4',
            'coroutine_resume5',
            'coroutine_resume6',
            'pcall1',
            'xpcall1',
        ],
        'programs' : [
            'sleepsort',
        ]
    }

    foreach suite, t : tests
        foreach t : t
            test(t, shell, suite : suite,
                 args : [
                     meson.current_source_dir() + '/test/run_test.sh',
                     meson.current_source_dir() + '/test/run_test.awk',
                     meson.current_source_dir() + '/test/' + t,
                 ],
                 env : [
# Test must override this env so non-colored terminal output will be
# auto-detected (this is, in itself, another test)
                     'EMILUA_COLORS=',
                     'EMILUA_BIN=' + emilua_bin.full_path(),
                     'AWK_BIN=' + gawk.path(),
                 ])
        endforeach
    endforeach
endif
