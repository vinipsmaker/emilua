fs = import('fs')

asciidoctor_diagram_args = []

if run_command(
    asciidoctor, '--require=asciidoctor-diagram', '--backend=manpage',
    '--doctype=manpage', '--out-file=-',
    'modules' / 'tutorial' / 'pages' / 'getting-started.adoc',
    check : false
).returncode() == 0
    asciidoctor_diagram_args += [
        '--require=asciidoctor-diagram',
        '--attribute=plantuml-format=utxt',
        '--attribute=ditaa-format=txt',
    ]
endif

foreach line : fs.read('modules' / 'ref' / 'all.adoc').split('\n')
    if not line.startswith('include::')
        continue
    endif
    input = (
        'modules' / 'ref' / line.replace('include::', '').replace('[]', ''))
    custom_target(
        fs.stem(input) + '.3em',
        build_by_default : true,
        command : [
            asciidoctor,
            '--backend=manpage',
            '--doctype=manpage',
            '--attribute=mansource=Emilua ' + meson.project_version() +
            get_option('version_suffix'),
            '--attribute=manmanual=Emilua reference',
            '--attribute=doctitle=@BASENAME@(3em)',
            asciidoctor_diagram_args,
            '--out-file=@OUTPUT@',
            '@INPUT@',
        ],
        output : '@BASENAME@.3em',
        input : input,
        install : true,
        install_dir : get_option('mandir') / 'man3em',
    )
endforeach

foreach line : fs.read('modules' / 'tutorial' / 'all.adoc').split('\n')
    if not line.startswith('include::')
        continue
    endif
    input = (
        'modules' / 'tutorial' /
        line.replace('include::', '').replace('[]', ''))
    custom_target(
        'emilua-' + fs.stem(input) + '.7',
        build_by_default : true,
        command : [
            asciidoctor,
            '--backend=manpage',
            '--doctype=manpage',
            '--attribute=mansource=Emilua ' + meson.project_version() +
            get_option('version_suffix'),
            '--attribute=manmanual=Emilua reference',
            '--attribute=doctitle=emilua-@BASENAME@(7)',
            asciidoctor_diagram_args,
            '--out-file=@OUTPUT@',
            '@INPUT@',
        ],
        output : 'emilua-@BASENAME@.7',
        input : input,
        install : true,
        install_dir : get_option('mandir') / 'man7',
    )
endforeach
