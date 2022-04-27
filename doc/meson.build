fs = import('fs')

src3 = [
    'modules/ref/pages/file_descriptor.adoc',
    'modules/ref/pages/readable_pipe.adoc',
    'modules/ref/pages/writable_pipe.adoc',
    'modules/ref/pages/pipe.pair.adoc',
    'modules/ref/pages/pipe.readable_from_fd.adoc',
    'modules/ref/pages/pipe.writable_from_fd.adoc',
    'modules/ref/pages/byte_span.adoc',
    'modules/ref/pages/cond.adoc',
    'modules/ref/pages/file.open_flag.adoc',
    'modules/ref/pages/file.random_access.adoc',
    'modules/ref/pages/file.stream.adoc',
    'modules/ref/pages/file.read_all_at.adoc',
    'modules/ref/pages/file.write_all_at.adoc',
    'modules/ref/pages/http.headers.adoc',
    'modules/ref/pages/http.request.adoc',
    'modules/ref/pages/http.response.adoc',
    'modules/ref/pages/http.socket.adoc',
    'modules/ref/pages/ip.address.adoc',
    'modules/ref/pages/ip.connect.adoc',
    'modules/ref/pages/ip.address_info_flag.adoc',
    'modules/ref/pages/ip.get_address_info.adoc',
    'modules/ref/pages/ip.get_name_info.adoc',
    'modules/ref/pages/ip.host_name.adoc',
    'modules/ref/pages/ip.message_flag.adoc',
    'modules/ref/pages/ip.tcp.acceptor.adoc',
    'modules/ref/pages/ip.tcp.socket.adoc',
    'modules/ref/pages/ip.udp.socket.adoc',
    'modules/ref/pages/json.adoc',
    'modules/ref/pages/json.writer.adoc',
    'modules/ref/pages/mutex.adoc',
    'modules/ref/pages/regex.adoc',
    'modules/ref/pages/serial_port.adoc',
    'modules/ref/pages/steady_timer.adoc',
    'modules/ref/pages/stream.write_all.adoc',
    'modules/ref/pages/stream.read_all.adoc',
    'modules/ref/pages/stream.scanner.adoc',
    'modules/ref/pages/system.arguments.adoc',
    'modules/ref/pages/system.environment.adoc',
    'modules/ref/pages/system.in_.adoc',
    'modules/ref/pages/system.out.adoc',
    'modules/ref/pages/system.err.adoc',
    'modules/ref/pages/system.exit.adoc',
    'modules/ref/pages/system.signal.adoc',
    'modules/ref/pages/system.signal.raise.adoc',
    'modules/ref/pages/system.signal.set.adoc',
    'modules/ref/pages/tls.context.adoc',
    'modules/ref/pages/tls.context_flag.adoc',
    'modules/ref/pages/tls.socket.adoc',
    'modules/ref/pages/unix.datagram_socket.adoc',
    'modules/ref/pages/unix.message_flag.adoc',
    'modules/ref/pages/unix.stream_acceptor.adoc',
    'modules/ref/pages/unix.stream_socket.adoc',
    'modules/ref/pages/websocket.adoc',
]

# Full-of-shit's Meson won't let me do away with the for-loop:
#
# ERROR: Output cannot contain @PLAINNAME@ or @BASENAME@ when there is more
# than one input (we can't know which to use)
foreach input : src3
    custom_target(
        'man3_' + fs.stem(input),
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

src7 = [
    'modules/tutorial/pages/actor.adoc',
    'modules/tutorial/pages/alternatives.adoc',
    'modules/tutorial/pages/fiber.adoc',
    'modules/tutorial/pages/HACKING.adoc',
    'modules/tutorial/pages/interruption.adoc',
    'modules/tutorial/pages/lua51.adoc',
    'modules/tutorial/pages/modules.adoc',
]

# Full-of-shit's Meson won't let me do away with the for-loop:
#
# ERROR: Output cannot contain @PLAINNAME@ or @BASENAME@ when there is more
# than one input (we can't know which to use)
foreach input : src7
    custom_target(
        'man7_' + fs.stem(input),
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
