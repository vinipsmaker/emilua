project(
    'emilua', 'cpp',
    default_options : ['cpp_std=c++20'],
    meson_version : '>=0.62.0',
    license : 'BSL-1.0',
    version : '0.3.2',
)

# https://github.com/mesonbuild/meson/issues/2320
subdir('include/emilua')

if get_option('thread_support_level') >= 1
    thread_dep = dependency('threads')
else
    thread_dep = dependency('', required : false)
endif

boost_modules = [
    'nowide',
]

if host_machine.system() == 'linux'
    boost_modules += 'context'
endif

if get_option('enable_linux_namespaces')
    boost_modules += 'serialization'
endif

luajit = dependency('luajit', version : '>=2.0.5')
boost = dependency(
    'boost',
    version : '>=1.80',
    modules : boost_modules,
)
fmt = dependency('fmt', version : '>=8.0')
serd = dependency('serd-0')
sord = dependency('sord-0')
trial_protocol = dependency('trial-protocol')
libdl = meson.get_compiler('cpp').find_library('dl', required : false)
gawk = find_program('gawk')
find_program('xxd')

add_project_arguments(
    '-DBOOST_DLL_USE_STD_FS',
    '-DBOOST_ASIO_DISABLE_CO_AWAIT',
    '-DBOOST_ASIO_DISABLE_STD_COROUTINE',
    '-DBOOST_BEAST_USE_STD_STRING_VIEW',
    language : 'cpp',
)

if meson.get_compiler('cpp').get_id() == 'msvc'
    if host_machine.cpu_family() != 'x86_64'
        error('Full LuaJIT C++ exception interoperability is required')
    endif

    if get_option('cpp_eh') != 'a'
        error('LuaJIT requires `/EHa` to have full C++ exception ' +
              'interoperability')
    endif

    add_project_arguments('/bigobj', language : 'cpp')
endif

if get_option('thread_support_level') == 0
    add_project_arguments('-DBOOST_ASIO_DISABLE_THREADS', language : 'cpp')
endif

if get_option('enable_io_uring')
    liburing = dependency('liburing', version : '>=2.0')
    add_project_arguments('-DBOOST_ASIO_HAS_IO_URING', language : 'cpp')
    add_project_arguments('-DBOOST_ASIO_DISABLE_EPOLL', language : 'cpp')
else
    liburing = dependency('', required : false)
endif

if host_machine.system() == 'linux'
    libcap = [dependency('libcap'), dependency('libpsx')]
else
    libcap = dependency('', required : false)
endif

if get_option('use_wolfssl')
    ssl = dependency('wolfssl')
    add_project_arguments('-DBOOST_ASIO_USE_WOLFSSL', language : 'cpp')
else
    ssl = dependency('openssl', version : '>=1.1.1')
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

re2c = find_program('re2c')
re2c_version = run_command([re2c, '--version'], check : true).stdout().split('\n')[0].split(' ')[-1].strip()
extra_re2c_args = []

if meson.get_compiler('cpp').get_id() == 'gcc' and re2c_version.version_compare('>=2.0')
    extra_re2c_args += '--case-ranges'
endif

re2c_args = [
    '-W', # Turn on all warnings
    '--flex-syntax',
    '--empty-class', 'match-none',
    '--input-encoding', 'ascii',
    extra_re2c_args,
    '-I', '@CURRENT_SOURCE_DIR@/include',
    '@INPUT@', '-o', '@OUTPUT@'
]

if re2c_version.version_compare('>=2.1')
    re2c_gen = generator(
        re2c,
        output: '@BASENAME@.cpp',
        depfile : '@PLAINNAME@.d',
        arguments: [
            '--depfile', '@DEPFILE@',
            re2c_args,
        ],
    )
else
    re2c_gen = generator(
        re2c,
        output: '@BASENAME@.cpp',
        arguments: re2c_args,
    )
endif

emilua_gperf = find_program(
    'emilua_gperf.awk',
    dirs : [meson.current_source_dir() / 'src'],
)

gperf_gen = generator(
    emilua_gperf,
    output : 'bc2_@BASENAME@.cpp',
    arguments: [
        '--assign', 'GPERF_BIN=' + find_program('gperf').full_path(),
        '@INPUT@', '@OUTPUT@',
    ],
)
install_data(
    'src' / 'emilua_gperf.awk',
    install_dir : get_option('datadir') / 'emilua',
)

if get_option('enable_manpages')
    asciidoctor = find_program('asciidoctor', native : true)
    custom_target(
        'manpages',
        build_by_default : true,
        command : [
            asciidoctor,
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

    subdir('doc')
endif

if get_option('disable_color')
    curses = dependency('', required : false)
else
    curses = dependency('curses', required : true)
endif

if get_option('enable_tests')
    if host_machine.system() == 'windows'
        shell = find_program(
            'run_test',
            dirs : meson.current_source_dir() / 'test',
        )
    else
        # GNU Coreutils binaries are not specified explicitly, but expected too
        shell = find_program('bash', 'ksh')
    endif
endif

incdir = [
    'include'
]

src = [
    'src/generic_error.cpp',
    'src/scope_cleanup.cpp',
    'src/serial_port.cpp',
    'src/async_base.cpp',
    'src/asio_error.cpp',
    'src/filesystem.cpp',
    'src/byte_span.cpp',
    'src/lua_shim.cpp',
    'src/stream.cpp',
    'src/system.cpp',
    'src/fiber.cpp',
    'src/mutex.cpp',
    'src/regex.cpp',
    'src/time.cpp',
    'src/condition_variable.cpp',
    'src/core.cpp',
    'src/json.cpp',
    'src/pipe.cpp',
    'src/tls.cpp',
    'src/ip.cpp',
]

bytecode_src = [
    'bytecode/async_base.lua',
    'bytecode/lua_shim.lua',
    'bytecode/stream.lua',
    'bytecode/actor.lua',
    'bytecode/fiber.lua',
    'bytecode/scope.lua',
    'bytecode/state.lua',
    'bytecode/condition_variable.lua',
    'bytecode/file.lua',
    'bytecode/json.lua',
    'bytecode/ip.lua',
]

re2c_src = [
    'src/actor.ypp',
    'src/main.ypp',
]

if host_machine.system() != 'windows'
    src += [
        'src/file_descriptor.cpp',
        'src/unix.cpp',
    ]
endif

if get_option('enable_plugins')
    src += 'src/plugin.cpp'
endif

if get_option('enable_http')
    warning('HTTP support is still experimental.' +
            ' Please report any bugs you find.')

    http_dep = dependency('emilua-http')
    src += [
        'src/websocket.cpp',
        'src/http.cpp',
    ]
else
    http_dep = dependency('', required : false)
endif

if get_option('enable_file_io')
    src += 'src/file.cpp'
endif

if get_option('enable_linux_namespaces')
    src += 'src/linux_namespaces.cpp'
endif

# Meson doesn't have the ability to chain generators, so instead of
# just calling gperf_gen.process(re2c_gen.process(re2c_src)), we have
# to manually write each target that needs a pipeline of generators
# https://github.com/mesonbuild/meson/issues/1141
pp_state = custom_target(
    'bc2_state.cpp',
    output : 'bc2_state.cpp',
    input : re2c_gen.process('src/state.ypp'),
    command : [
        emilua_gperf,
        '--assign', 'GPERF_BIN=' + find_program('gperf').full_path(),
        '@INPUT@', '@OUTPUT@',
    ],
)

emilua_bin = executable(
    'emilua',
    gperf_gen.process(src),
    bytecode_gen.process(bytecode_src),
    re2c_gen.process(re2c_src),
    pp_state,
    cpp_pch : 'pch/pchheader.hpp',
    dependencies : [
        thread_dep,
        boost,
        luajit,
        fmt,
        serd,
        sord,
        trial_protocol,
        http_dep,
        ssl,
        curses,
        libdl,
        liburing,
        libcap,
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

if get_option('enable_io_uring')
    pc_extra_cflags += [
        '-DBOOST_ASIO_HAS_IO_URING',
        '-DBOOST_ASIO_DISABLE_EPOLL',
    ]
endif

if get_option('use_wolfssl')
    pc_extra_cflags += '-DBOOST_ASIO_USE_WOLFSSL'
endif

import('pkgconfig').generate(
    filebase : 'emilua',
    name : 'Emilua Plugin API',
    description : 'Lua execution engine',
    requires : [
        luajit.name(),
        fmt.name(),
        ssl.name(),
    ] +
    (get_option('enable_io_uring') ? [liburing.name()] : []),
    variables : [
        'pluginsdir=${libdir}' / 'emilua-' +
        meson.project_version().split('.')[0] + '.' +
        meson.project_version().split('.')[1],
        'thread_support_level=' +
        get_option('thread_support_level').to_string(),
        'file_io_enabled=' + get_option('enable_file_io').to_string(),
        'emilua_gperf=${datadir}' / 'emilua' / 'emilua_gperf.awk',
    ],
    extra_cflags : pc_extra_cflags,
)

if get_option('enable_tests')
    normalize_path_bin = executable(
        'normalize_path',
        'tool/normalize_path.cpp',
        dependencies : [
            boost,
            luajit,
        ],
        implicit_include_directories : false,
    )

    tests_env = [
        # Test must override this env so non-colored terminal output will be
        # auto-detected (this is, in itself, another test)
        'EMILUA_COLORS=',

        'EMILUA_BIN=' + emilua_bin.full_path(),
        'NORMALIZE_PATH_BIN=' + normalize_path_bin.full_path(),
        'AWK_BIN=' + gawk.full_path(),

        # Make output strings for perror(), strerror(), and others predictable
        'LANG=',
        'LC_ALL=',
        'LC_MESSAGES=',
    ]

    if host_machine.system() == 'windows'
        tests_env += ('APPVEYOR_FIXUP=' +
            meson.current_source_dir() / 'test' / 'force_windows.awk')
    endif

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
        'byte_span' : [
            # new(), __len(), capacity
            'byte_span1',
            'byte_span2',
            'byte_span3',
            'byte_span4',
            'byte_span5',
            'byte_span6',

            # slice(), __index(), __newindex(), __eq()
            'byte_span7',
            'byte_span8',
            'byte_span9',
            'byte_span10',
            'byte_span11',
            'byte_span12',
            'byte_span13',
            'byte_span14',

            # copy(),  __tostring()
            'byte_span15',

            # append()
            'byte_span16',

            # string algorithms
            'byte_span17',
            'byte_span18',
            'byte_span19',
        ],
        'regex' : [
            'regex1',
            'regex2',
            'regex3',
            'regex4',
            'regex5',
            'regex6',
            'regex7',
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
                'http5',
            ]
        }
    endif

    if get_option('enable_linux_namespaces')
        tests +=  {
            'linux_namespaces' : [
                # serialization for good objects
                'linux_namespaces1',
                'linux_namespaces2',
                'linux_namespaces3',
                'linux_namespaces4',
                'linux_namespaces5',
                'linux_namespaces6',
                'linux_namespaces7',
                'linux_namespaces8',
                'linux_namespaces9',
                'linux_namespaces10',
                'linux_namespaces11',
                'linux_namespaces12',
                'linux_namespaces13',
                'linux_namespaces14',
                'linux_namespaces15',
                'linux_namespaces16',
                'linux_namespaces17',
                'linux_namespaces18',
                'linux_namespaces19',
                'linux_namespaces20',
                'linux_namespaces21',
                'linux_namespaces22',
                'linux_namespaces23',
                'linux_namespaces24',
                'linux_namespaces25',
                'linux_namespaces26',
                'linux_namespaces27',
                'linux_namespaces28',
                'linux_namespaces29',
                'linux_namespaces30',
                'linux_namespaces31',
                'linux_namespaces32',
                'linux_namespaces33',
                'linux_namespaces34',
                'linux_namespaces35',
                'linux_namespaces36',
                'linux_namespaces37',
                'linux_namespaces38',
                'linux_namespaces39',
                'linux_namespaces40',
                'linux_namespaces41',
                'linux_namespaces42',
                'linux_namespaces43',
                'linux_namespaces44',

                # bad input on send()
                'linux_namespaces45',
                'linux_namespaces46',
                'linux_namespaces47',
                'linux_namespaces48',
                'linux_namespaces49',
                'linux_namespaces50',
                'linux_namespaces51',
                'linux_namespaces52',
                'linux_namespaces53',
                'linux_namespaces54',
                'linux_namespaces55',

                # misc
                'linux_namespaces56',
            ]
        }
    endif

    if get_option('enable_linux_namespaces') and get_option('enable_plugins')
        linux_namespaces_badinjector = shared_module(
            'linux_namespaces_badinjector',
            ['test/linux_namespaces_badinjector.cpp'],
            dependencies : [
                thread_dep,
                boost,
                luajit,
            ],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
            gnu_symbol_visibility : 'hidden',
        )

        tests_env += (
            'EMILUA_PATH=' +
            fs.parent(linux_namespaces_badinjector.full_path()))

        tests +=  {
            'linux_namespaces2' : [
                'linux_namespaces2_1',
                'linux_namespaces2_2',
                'linux_namespaces2_3',
                'linux_namespaces2_4',
                'linux_namespaces2_5',
                'linux_namespaces2_6',
                'linux_namespaces2_7',
                'linux_namespaces2_8',
                'linux_namespaces2_9',
                'linux_namespaces2_10',
                'linux_namespaces2_11',
                'linux_namespaces2_12',
                'linux_namespaces2_13',
                'linux_namespaces2_14',
                'linux_namespaces2_15',
                'linux_namespaces2_16',
                'linux_namespaces2_17',
                'linux_namespaces2_18',
                'linux_namespaces2_19',
                'linux_namespaces2_20',
                'linux_namespaces2_21',
                'linux_namespaces2_22',

                'linux_namespaces_fuzzer1',
                'linux_namespaces_fuzzer2',
            ]
        }
    endif

    if host_machine.system() == 'linux'
        tests +=  {
            'module_system2' : [
                # EIO on /proc/self/mem is a Linux trick
                'module8',
            ]
        }
    endif

    foreach suite, t : tests
        foreach t : t
            test(t, shell, suite : suite,
                 args : [
                     meson.current_source_dir() / 'test' / 'run_test.sh',
                     meson.current_source_dir() / 'test' / 'run_test.awk',
                     meson.current_source_dir() / 'test' / t,
                 ],
                 env : tests_env)
        endforeach
    endforeach
endif

if get_option('enable_gperf_tests')
    gperf_tests = [
        'gperf_basic',
        'gperf_nested',
        'gperf_nested2',
        'gperf_nested3',
    ]
    foreach t : gperf_tests
        test(
            t, executable(
                t,
                gperf_gen.process('test' / t + '.cpp'),
                dependencies : [ fmt ],
                include_directories : include_directories(
                    'test', #< https://github.com/mesonbuild/meson/issues/9219
                ),
                implicit_include_directories : false,
            ),
            suite : 'gperf',
            protocol : 'tap',
        )
    endforeach
endif
