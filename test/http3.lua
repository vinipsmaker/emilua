local ip = require 'ip'
local http = require 'http'
local ok, socket_pair = pcall(require, 'unix')
if ok then
    socket_pair = socket_pair.stream_socket.pair
else
    socket_pair = require('./util').socket_pair
end

local a, b = socket_pair()
a, b = http.socket.new(a), http.socket.new(b)

local f = spawn(function()
    local sock, req, res = b, http.request.new(), http.response.new()

    req.method = 'POST'
    req.target = '/foobar'
    req.headers = {
        host = '127.0.0.1',
    }
    req.body = 'foobar2'
    sock:write_request_metadata(req)
    sock:write(req)
    sock:write_end_of_message()

    sock:read_response(res)
    while sock.read_state ~= 'empty' do
        sock:read_some(res)
    end
    sock:close()

    print(res.status)
    print(res.reason)
    print(res.body)
    print(res.trailers['x-md5'])
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
print(req.body)

res.status = 200
res.reason = 'OK'
res.body = 'foobar'
res.trailers['x-md5'] = '3858f62230ac3c915f300c664312c63f'
sock:write_response_metadata(res)
sock:write(res)
sock:write_trailers(res)
