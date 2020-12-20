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

local acceptor = ip.tcp.acceptor.new()
acceptor:open('v4')
acceptor:set_option('reuse_address', true)
acceptor:bind(ip.address.loopback_v4(), 8080)
acceptor:listen()

while true do
    local sock = http.socket.new(acceptor:accept())
    spawn(function()
        local req = http.request.new()
        local res = http.response.new()

        sock:read_request(req)
        sock:write_response_continue()

        print(req.method)
        print(req.target)
        print_headers(req.headers)

        while sock.read_state ~= 'finished' do
            req.body = nil --< discard unused data
            sock:read_some(req)
        end

        res.status = 200
        res.reason = 'OK'
        res.body = 'Found\n'
        sock:write_response(res)
    end)
end
