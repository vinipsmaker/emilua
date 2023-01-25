-- serialization/bad
local NITER = 35000

local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local badinjector = require 'linux_namespaces_badinjector'
local stream = require 'stream'
local system = require 'system'
local inbox = require 'inbox'
local unix = require 'unix'

local guest_code = [[
    local stream = require 'stream'
    local system = require 'system'
    local inbox = require 'inbox'
    local json = require 'json'
    local unix = require 'unix'

    local host = inbox:receive()
    local NITER = inbox:receive()
    local utf8_converter = unix.seqpacket_socket.new(inbox:receive())

    local function to_base64(value)
        if #value == 0 then
            return ''
        end
        utf8_converter:send(byte_span.append(value))
        local buf = byte_span.new(8192)
        local nread = utf8_converter:receive(buf)
        return tostring(buf:slice(1, nread))
    end

    local function stringize(value)
        if type(value) == 'table' then
            for k, v in pairs(value) do
                if type(v) == 'string' then
                    value[k] = 's:' .. v
                elseif getmetatable(v) == 'file_descriptor' then
                    value[k] = tostring(v)
                elseif getmetatable(v) == 'linux_container_channel' then
                    value[k] = 'actor_address'
                end
            end
            return json.encode(value)
        elseif type(value) == 'string' then
            return '"' .. to_base64(value) .. '"'
        elseif getmetatable(value) == 'linux_container_channel' then
            return 'actor_address'
        end
        return tostring(value)
    end

    for i = 1, NITER do
        host:send(inbox)
        local ok, value = pcall(function()
            return inbox:receive()
        end)
        if ok or value.code ~= 16 then --< errc::no_senders
            if ok then
                value = stringize(value)
            else
                value = '"' .. tostring(value) .. '"'
            end
            -- bypass buffered IO
            stream.write_all(
                system.err,
                'FAIL i=' .. i ..
                (ok and ' receive()=' or ' EC=') ..
                value .. '\n')
            system.exit(1)
        end
    end

    host:send('OK')
]]

if system.environment.EMILUA_TEST_SEED then
    local seed = system.environment.EMILUA_TEST_SEED + 0
    badinjector.fuzzer_seed(seed)
    print('SEED=' .. seed)
else
    print('SEED=' .. badinjector.fuzzer_seed())
end

if system.environment.EMILUA_TEST_FUZZER_NITER then
    NITER = system.environment.EMILUA_TEST_FUZZER_NITER + 0
end

if system.environment.EMILUA_TEST_FUZZER_SKIP then
    local n = system.environment.EMILUA_TEST_FUZZER_SKIP + 0
    for i = 1, n do
        badinjector.fuzzer_send_bad()
    end
end

local my_channel = spawn_vm(guest_code)
my_channel:send(inbox)
my_channel:send(NITER)
do
    local utf8_converter = {unix.seqpacket_socket.pair()}
    utf8_converter[2] = utf8_converter[2]:release()
    my_channel:send(utf8_converter[2])
    utf8_converter[2]:close()
    utf8_converter = utf8_converter[1]
    spawn(function()
        scope_cleanup_push(function() utf8_converter:close() end)
        local to_base64 = badinjector.to_base64
        local buf = byte_span.new(8192)
        while true do
            local ok = pcall(function()
                local nread = utf8_converter:receive(buf)
                local result = to_base64(tostring(buf:slice(1, nread)))
                local size = buf:copy(result)
                utf8_converter:send(buf:slice(1, size))
            end)
            if not ok then
                return
            end
        end
    end):detach()
end
my_channel:close()

for i = 1, NITER do
    local ok, e = pcall(function()
        local ch = inbox:receive()
        badinjector.fuzzer_send_bad(ch)
        ch:close()
    end)
    if not ok then
        stream.write_all(system.err, 'HOST FAIL i=' .. i .. '\n')
        error(e)
    end
end

print(inbox:receive())
