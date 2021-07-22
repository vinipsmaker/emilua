project(
    'emilua', 'cpp',
    default_options : ['cpp_std=c++17'],
    meson_version : '>=0.55.0',
    license : 'BSL-1.0',
    version : '0.3.2',
)

# https://github.com/mesonbuild/meson/issues/2320
subdir('include/emilua')

boost_modules = []

if get_option('thread_support_level') >= 1
    boost_modules += 'thread'
endif

luajit = dependency('luajit', version : '>=2.0.5')
boost = dependency(
    'boost',
    version : '>=1.72',
    modules : boost_modules,
    static : true
)
fmt = dependency('fmt', version : '>=6.2.1')
libdl = meson.get_compiler('cpp').find_library('dl', required : false)
ssl = dependency('openssl', version : '>=1.1.1')
find_program('xxd')

add_project_arguments(
    '-DBOOST_DLL_USE_STD_FS',
    '-DBOOST_ASIO_NO_DYNAMIC_BUFFER_V1',
    '-DBOOST_ASIO_DISABLE_CO_AWAIT',
    '-DBOOST_ASIO_DISABLE_STD_COROUTINE',
    language : 'cpp',
)

if get_option('thread_support_level') == 0
    add_project_arguments('-DBOOST_ASIO_DISABLE_THREADS', language : 'cpp')
endif

bytecode_gen = generator(
    find_program(
        'luajit',
        dirs: [
            luajit.get_variable(pkgconfig : 'prefix', default_value : '') /
            'bin'
        ]
    ),
    output: 'bc_@BASENAME@.cpp',
    arguments: ['--', '@INPUT@', '@OUTPUT@']
)

re2c_gen = generator(
    find_program('re2c'),
    output: '@BASENAME@.cpp',
    # TODO: enable depfile on next re2c release
    #depfile : '@PLAINNAME@.d',
    arguments: [
        '-W', # Turn on all warnings
        '--flex-syntax',
        '--case-ranges',
        '--empty-class', 'match-none',
        '--input-encoding', 'ascii',
        '-I', '@CURRENT_SOURCE_DIR@/include',
        #'--depfile', '@DEPFILE@',
        '@INPUT@', '-o', '@OUTPUT@'
    ]
)

if get_option('enable_manpages')
    custom_target(
        'manpages',
        build_by_default : true,
        command : [
            find_program('asciidoctor', native : true),
            '--backend=manpage',
            '--attribute=VERSION=' + meson.project_version() +
            get_option('version_suffix'),
            '--out-file=@OUTPUT@',
            '@INPUT@',
        ],
        output : '@BASENAME@',
        input : 'doc/man/emilua.1.adoc',
        install : true,
        install_dir : get_option('mandir') / 'man1',
    )
endif

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
    'include'
]

src = [
    'src/scope_cleanup.cpp',
    'src/lua_shim.cpp',
    'src/fiber.cpp',
    'src/mutex.cpp',
    'src/timer.cpp',
    'src/cond.cpp',
    'src/core.cpp',
    'src/json.cpp',
    'src/sys.cpp',
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
    'src/actor.ypp',
    'src/state.ypp',
    'src/main.ypp',
]

if get_option('enable_plugins')
    src += 'src/plugin.cpp'
endif

if get_option('enable_http')
    warning('HTTP support is still experimental.' +
            ' Please report any bugs you find.')

    incdir += '3rd/boost.http/include'
    src += 'src/http.cpp'
    bytecode_src += 'bytecode/http.lua'
endif

emilua_bin = executable(
    'emilua',
    src,
    bytecode_gen.process(bytecode_src),
    re2c_gen.process(re2c_src),
    cpp_pch : 'pch/pchheader.hpp',
    dependencies : [
        boost,
        luajit,
        fmt,
        ssl,
        curses,
        libdl,
    ],
    export_dynamic : get_option('enable_plugins'),
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    install : true,
)

install_subdir(
    'include/emilua',
    install_dir : get_option('includedir'),
    exclude_files : [
        'config.h.in',
        'meson.build',
    ],
)

pc_extra_cflags = []

if get_option('thread_support_level') == 0
    pc_extra_cflags += '-DBOOST_ASIO_DISABLE_THREADS'
endif

import('pkgconfig').generate(
    filebase : 'emilua',
    name : 'Emilua Plugin API',
    description : 'Lua execution engine',
    requires : [
        luajit.name(),
        fmt.name(),
    ],
    variables : [
        'pluginsdir=${libdir}' / 'emilua-' +
        meson.project_version().split('.')[0] + '.' +
        meson.project_version().split('.')[1],
    ],
    extra_cflags : pc_extra_cflags,
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
            'coroutine_resume7',
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
            'module20',
            'module21',
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
            'actor27',
            'actor28',
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

    if get_option('thread_support_level') >= 2
        tests +=  {
            'actor' : tests['actor'] + [
                'actor29',
            ]
        }
    endif

    if get_option('enable_http')
        tests +=  {
            'http' : [
                'http1',
                'http2',
                'http3',
                'http4',
            ]
        }
    endif

    foreach suite, t : tests
        foreach t : t
            test(t, shell, suite : suite,
                 args : [
                     meson.current_source_dir() + '/test/run_test.sh',
                     meson.current_source_dir() + '/test/run_test.awk',
                     meson.current_source_dir() + '/test/' + t,
                 ],
                 env : [
                     # Test must override this env so non-colored terminal
                     # output will be auto-detected (this is, in itself, another
                     # test)
                     'EMILUA_COLORS=',
                     'EMILUA_BIN=' + emilua_bin.full_path(),
                     'AWK_BIN=' + gawk.full_path(),
                 ])
        endforeach
    endforeach
endif
