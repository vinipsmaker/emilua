project('emilua', 'cpp', default_options : ['cpp_std=c++17'])

conf = configuration_data()
conf.set10('ENABLE_COLOR', not get_option('disable_color'))
conf.set10('ENABLE_HTTP', get_option('enable_http'))
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
ssl = dependency('openssl', version : '>=1.1.1')
find_program('xxd')
bytecode_gen = generator(
    find_program(
        'luajit',
        dirs: [luajit.get_pkgconfig_variable('prefix') + '/bin']
    ),
    output: 'bc_@BASENAME@.cpp',
    arguments: ['--', '@INPUT@', '@OUTPUT@']
)
re2c_gen = generator(
    find_program('re2c'),
    output: '@BASENAME@.cpp',
    arguments: [
        '-W', # Turn on all warnings
        '--flex-syntax',
        '--case-ranges',
        '--empty-class', 'match-none',
        '--input-encoding', 'ascii',
        '@INPUT@', '-o', '@OUTPUT@'
    ]
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

incdir = [
    '3rd/trial.protocol/include',
    '3rd/CLI11/include',
    '3rd/fmt/include',
    'include'
]

src = [
    '3rd/fmt/src/format.cc',

    'src/scope_cleanup.cpp',
    'src/lua_shim.cpp',
    'src/fiber.cpp',
    'src/actor.cpp',
    'src/mutex.cpp',
    'src/timer.cpp',
    'src/cond.cpp',
    'src/core.cpp',
    'src/json.cpp',
    'src/main.cpp',
    'src/tls.cpp',
    'src/ip.cpp',
]

bytecode_src = [
    'bytecode/lua_shim.lua',
    'bytecode/actor.lua',
    'bytecode/fiber.lua',
    'bytecode/scope.lua',
    'bytecode/state.lua',
    'bytecode/timer.lua',
    'bytecode/cond.lua',
    'bytecode/json.lua',
    'bytecode/tls.lua',
    'bytecode/ip.lua',
]

re2c_src = [
    'src/state.ypp',
]

if get_option('enable_http')
    incdir += '3rd/boost.http/include'
    src += 'src/http.cpp'
    bytecode_src += 'bytecode/http.lua'
endif

emilua_bin = executable(
    'emilua',
    src,
    bytecode_gen.process(bytecode_src),
    re2c_gen.process(re2c_src),
    cpp_pch : 'meson/pchheader.hpp',
    dependencies : [boost, luajit, ssl, curses],
    include_directories : include_directories(incdir)
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
            'interrupt4',
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
        'module_system' : [
            'module1',
            'module2',
            'module3',
            'module4',
            'module5',
            'module6',
            'module7',
            'module8',
            'module9',
            'module10',
            'module11',
            'module12',
            'module13',
            'module14',
            'module15',
            'module16',
            'module17',
            'module18',
            'module19',
            'non-portable/module1',
        ],
        'actor' : [
            'actor1',
            'actor2',
            'actor3',
            'actor4',
            'actor5',
            'actor6',
            'actor7',
            'actor8',
            'actor9',
            'actor10',
            'actor11',
            'actor12',
            'actor13',
            'actor14',
            'actor15',
            'actor16',
            'actor17',
            'actor18',
            'actor19',
            'actor20',
            'actor21',
            'actor22',
            'actor23',
            'actor24',
            'actor25',
            'actor26',
        ],
        'json' : [
            'json1',
            'json2',
            'json3',
            'json4',
            'json5',
            'json6',
            'json7',
            'json8',
            'json9',
            'json10',
            'json11',
            'json12',
            'json13',
            'json14',
        ],
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
                     'AWK_BIN=' + gawk.full_path(),
                 ])
        endforeach
    endforeach
endif
