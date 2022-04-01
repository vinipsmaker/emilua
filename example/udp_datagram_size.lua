-- this is usually a very bad idea
local ip = require 'ip'

local sock = ip.udp.socket.new()
sock:open('v4')
sock:bind(ip.address.any_v4(), 1234)

while true do
    sock:receive(byte_span.new(0), ip.message_flag.peek)
    local buf = byte_span.new(sock:io_control('bytes_readable'))
    local nread, remote_addr, remote_port = sock:receive(buf)
    print('from: ' .. tostring(remote_addr) .. ':' .. remote_port .. ':')
    print(buf)
end
