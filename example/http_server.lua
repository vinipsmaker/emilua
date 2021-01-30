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
if not pcall(function() acceptor:bind(ip.address.loopback_v4(), 8080) end) then
    acceptor:bind(ip.address.loopback_v4(), 0)
end
print('Listening on ' .. tostring(acceptor.local_address) .. ':' ..
      acceptor.local_port)
acceptor:listen()

while true do
    local sock = http.socket.new(acceptor:accept())
    spawn(function()
        local req = http.request.new()
        local res = http.response.new()

        res.status = 200
        res.reason = 'OK'
        res.body = 'Hello World\n'

        while true do
            sock:read_request(req)
            if http.request.continue_required(req) then
                sock:write_response_continue()
            end

            print(req.method .. ' ' .. req.target)
            print_headers(req.headers)

            while sock.read_state ~= 'finished' do
                req.body = nil --< discard unused data
                sock:read_some(req)
            end

            sock:write_response(res)
        end
    end):detach()
end
