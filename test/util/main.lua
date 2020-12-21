local ip = require 'ip'

function socket_pair()
    local acceptor = ip.tcp.acceptor.new()
    local addr = ip.address.loopback_v4()
    acceptor:open(addr)
    acceptor:bind(addr, 0)
    acceptor:listen()

    local f = spawn(function()
        local sock = ip.tcp.socket.new()
        sock:connect(addr, acceptor.local_port)
        return sock
    end)

    local sock = acceptor:accept()
    acceptor:close()
    return sock, f:join()
end
