fs = import('fs')

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
            '--out-file=@OUTPUT@',
            '@INPUT@',
        ],
        output : 'emilua-@BASENAME@.7',
        input : input,
        install : true,
        install_dir : get_option('mandir') / 'man7',
    )
endforeach
