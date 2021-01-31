local ip = require 'ip'
local http = require 'http'
local socket_pair = require('./util').socket_pair

local a, b = socket_pair()
a, b = http.socket.new(a), http.socket.new(b)

local f = spawn(function()
    local sock, req, res = b, http.request.new(), http.response.new()

    req.method = 'GET'
    req.target = '/foobar'
    req.headers = {
        host = '127.0.0.1',
        ['x-foobar'] = 'foobar2'
    }
    sock:lock_client_to_http10()
    sock:write_request(req)

    sock:read_response(res)
    while sock.read_state ~= 'empty' do
        sock:read_some(res)
    end

    print(res.status)
    print(res.reason)
    print(res.body)
end)

local sock, req, res = a, http.request.new(), http.response.new()

sock:read_request(req)
if http.request.continue_required(req) then
    error('query algorithm is drunk')
end
while sock.read_state ~= 'finished' do
    sock:read_some(req)
end

print(req.method)
print(req.target)
print(req.headers['x-foobar'])

res.status = 200
res.reason = 'OK'
res.body = 'foobar'
sock:write_response(res)
