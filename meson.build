project('emilua', 'cpp', default_options : ['cpp_std=c++17'])

luajit = dependency('luajit', version : '>=2.0.5')
boost = dependency(
    'boost',
    version : '>=1.69',
    modules : ['thread'],
    static : true
)

incdir = include_directories([
    '3rd/CLI11/include',
    'include'
])

src = [
    'src/state.cpp',
    'src/core.cpp',
    'src/main.cpp'
]

executable(
    'emilua',
    src,
    cpp_pch : 'pch/pchheader.hpp',
    dependencies : [boost, luajit],
    include_directories : incdir
)
