local ip = require 'ip'

local acceptor = ip.tcp.acceptor.new()
acceptor:open('v4')
acceptor:set_option('reuse_address', true)
acceptor:bind(ip.address.loopback_v4(), 1234)
acceptor:listen()

local sock = acceptor:accept()
sock:set_option('out_of_band_inline', true)

local buf = byte_span.new(1024)
local nread

sock:wait('read')
if sock.at_mark then
    print('received OOB inline:')
else
    print('received data:')
end
nread = sock:read_some(buf)
print(buf:slice(1, nread))

sock:wait('read')
if sock.at_mark then
    print('received OOB inline:')
else
    print('received data:')
end
nread = sock:read_some(buf)
print(buf:slice(1, nread))
