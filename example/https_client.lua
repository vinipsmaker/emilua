local ip = require 'ip'
local stream = require 'stream'
local http = require 'http'
local tls = require 'tls'

tls_ctx = tls.context.new('tlsv13')
tls_ctx:set_default_verify_paths()

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
local endpoints = ip.tcp.get_address_info(host, 'https')

print('Connecting...')
stream.connect(sock, endpoints)
sock:set_option('tcp_no_delay', true)
sock = tls.socket.new(sock, tls_ctx)
sock:set_verify_mode('peer');
sock:set_verify_callback('host_name_verification', host);
sock:set_sni_client_hostname(host)
sock:client_handshake()
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
