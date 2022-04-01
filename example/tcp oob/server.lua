local ip = require 'ip'

local acceptor = ip.tcp.acceptor.new()
acceptor:open('v4')
acceptor:set_option('reuse_address', true)
acceptor:bind(ip.address.loopback_v4(), 1234)
acceptor:listen()

local sock = acceptor:accept()

spawn(function()
    local buf = byte_span.new(1)
    sock:receive(buf, ip.message_flag.out_of_band)
    print('received OOB: ' .. tostring(buf))
end):detach()

local buf = byte_span.new(1024)
local nread = sock:read_some(buf)
print('received data:')
print(buf:slice(1, nread))
