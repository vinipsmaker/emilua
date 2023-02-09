local ip = require 'ip'
local sleep = require 'sleep'

local sock = ip.tcp.socket.new()
sock:connect(ip.address.loopback_v4(), 1234)

sleep(0.5)
sock:send(byte_span.append('a'), ip.message_flag.out_of_band)

sleep(2)
sock:write_some(byte_span.append('test'))
