local stream = require 'stream'
local system = require 'system'
local file = require 'file'
local unix = require 'unix'

local init_script = [[
    local uidmap = C.open('/proc/self/uid_map', C.O_WRONLY)
    send_with_fd(fdarg, '.', uidmap)
    C.write(C.open('/proc/self/setgroups', C.O_WRONLY), 'deny')
    local gidmap = C.open('/proc/self/gid_map', C.O_WRONLY)
    send_with_fd(fdarg, '.', gidmap)

    -- sync point #1 as tmpfs will fail on mkdir()
    -- with EOVERFLOW if no UID/GID mapping exists
    -- https://bugzilla.kernel.org/show_bug.cgi?id=183461
    C.read(fdarg, 1)

    local userns = C.open('/proc/self/ns/user', C.O_RDONLY)
    send_with_fd(fdarg, '.', userns)
    local netns = C.open('/proc/self/ns/net', C.O_RDONLY)
    send_with_fd(fdarg, '.', netns)

    -- unshare propagation events
    C.mount(nil, '/', nil, C.MS_PRIVATE)

    C.mount(nil, '/mnt', 'tmpfs', 0)
    C.mkdir('/mnt/proc', mode(7, 5, 5))
    C.mount(nil, '/mnt/proc', 'proc', 0)
    C.mkdir('/mnt/tmp', mode(7, 7, 7))

    -- pivot root
    C.mkdir('/mnt/mnt', mode(7, 5, 5))
    C.chdir('/mnt')
    C.pivot_root('.', '/mnt/mnt')
    C.chroot('.')
    C.umount2('/mnt', C.MNT_DETACH)

    local modulefd = C.open(
        '/app.lua',
        bit.bor(C.O_WRONLY, C.O_CREAT),
        mode(6, 0, 0))
    send_with_fd(fdarg, '.', modulefd)

    -- sync point #2
    C.read(fdarg, 1)

    C.sethostname('localhost')
    C.setdomainname('localdomain')

    -- drop all root privileges
    C.cap_set_proc('=')
]]

local do_spawn_vm = spawn_vm

function spawn_vm(guest_code)
    local shost, sguest = unix.seqpacket_socket.pair()
    sguest = sguest:release()

    local my_channel = do_spawn_vm('/app.lua', {
        linux_namespaces = {
            new_user = true,
            new_net = true,
            new_mount = true,
            new_pid = true,
            new_uts = true,
            new_ipc = true,
            stdout = 'share',
            stderr = 'share',
            init = { script = init_script, fd = sguest }
        }
    })
    sguest:close()

    local ignored_buf = byte_span.new(1)

    local uidmap = ({system.getresuid()})[2]
    uidmap = byte_span.append('0 ', tostring(uidmap), ' 1\n')
    local uidmapfd = ({shost:receive_with_fds(ignored_buf, 1)})[2][1]
    file.stream.new(uidmapfd):write_some(uidmap)

    local gidmap = ({system.getresgid()})[2]
    gidmap = byte_span.append('0 ', tostring(gidmap), ' 1\n')
    local gidmapfd = ({shost:receive_with_fds(ignored_buf, 1)})[2][1]
    file.stream.new(gidmapfd):write_some(gidmap)

    -- sync point #1
    shost:send(ignored_buf)

    local userns = ({shost:receive_with_fds(ignored_buf, 1)})[2][1]
    local netns = ({shost:receive_with_fds(ignored_buf, 1)})[2][1]
    system.spawnp{
        program = 'ip',
        arguments = {'ip', 'link', 'set', 'dev', 'lo', 'up'},
        nsenter_user = userns,
        nsenter_net = netns
    }:wait()

    local module = ({shost:receive_with_fds(ignored_buf, 1)})[2][1]
    module = file.stream.new(module)
    stream.write_all(module, guest_code)

    -- sync point #2
    shost:close()

    return my_channel
end
