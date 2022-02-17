local ip = require 'ip'
local http = require 'http'

local function print_headers(headers)
    for k, v in pairs(headers) do
        if type(v) == 'table' then
            for _, vi in ipairs(v) do
                print(' ' .. k .. ': ' .. vi)
            end
        else
            print(' ' .. k .. ': ' .. v)
        end
    end
end

local host = 'example.com'

local sock = ip.tcp.socket.new()
local req = http.request.new()
local res = http.response.new()

print('Resolving ' .. host .. '...')
local addr = ip.tcp.resolver.new():resolve(host, '')[1].address

print('Connecting to ' .. tostring(addr))
sock:connect(addr, 80)
sock = http.socket.new(sock)

req.method = 'GET'
req.target = '/'
req.headers.host = host
sock:write_request(req)
sock:read_response(res)
print(res.status .. ' ' .. res.reason)
print_headers(res.headers)

while sock.read_state ~= 'empty' do
    print('reading more body')
    sock:read_some(res)
end
