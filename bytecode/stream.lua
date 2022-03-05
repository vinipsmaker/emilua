OUTPUT = arg[#arg]

function strip_xxd_hdr(file)
    local lines = {}
    for line in file:lines() do
        lines[#lines + 1] = line
    end
    table.remove(lines, 1)
    lines[#lines] = nil
    lines[#lines] = nil
    return table.concat(lines)
end

function connect_bootstrap(error, type, next, pcall, e_not_found, e_invalid)
    return function(sock, resolve_results, condition)
        local t_condition = type(condition)
        if
            type(resolve_results) ~= 'table' or
            (t_condition ~= 'nil' and t_condition ~= 'function')
        then
            error(e_invalid)
        end

        local last_error
        for _, v in next, resolve_results do
            if condition and not condition(last_error, v.address, v.port) then
                goto continue
            end

            if sock.is_open then sock:close() end
            local ok, e = pcall(function() sock:connect(v.address, v.port) end)
            if ok then
                return v.address, v.port
            end
            last_error = e

            ::continue::
        end
        if last_error == nil then
            last_error = e_not_found
        end
        error(last_error)
    end
end

connect_bytecode = string.dump(connect_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(connect_bytecode)
f:close()
connect_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function write_bootstrap(stream, buffer)
   local ret = #buffer
   while #buffer > 0 do
       local nwritten = stream:write_some(buffer)
       buffer = buffer:slice(1 + nwritten)
   end
   return ret
end

write_bytecode = string.dump(write_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(write_bytecode)
f:close()
write_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function read_bootstrap(stream, buffer)
    local ret = #buffer
    while #buffer > 0 do
        local nread = stream:read_some(buffer)
        buffer = buffer:slice(1 + nread)
    end
    return ret
end

read_bytecode = string.dump(read_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(read_bytecode)
f:close()
read_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function write_at_bootstrap(io_obj, offset, buffer)
   local ret = #buffer
   while #buffer > 0 do
       local nwritten = io_obj:write_some_at(offset, buffer)
       offset = offset + nwritten
       buffer = buffer:slice(1 + nwritten)
   end
   return ret
end

write_at_bytecode = string.dump(write_at_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(write_at_bytecode)
f:close()
write_at_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function read_at_bootstrap(io_obj, offset, buffer)
    local ret = #buffer
    while #buffer > 0 do
        local nread = io_obj:read_some_at(offset, buffer)
        offset = offset + nread
        buffer = buffer:slice(1 + nread)
    end
    return ret
end

read_at_bytecode = string.dump(read_at_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(read_at_bytecode)
f:close()
read_at_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char stream_connect_bytecode[] = {
]])

f:write(connect_cdef)

f:write('};')
f:write(string.format('std::size_t stream_connect_bytecode_size = %i;', #connect_bytecode))

f:write([[
unsigned char write_bytecode[] = {
]])

f:write(write_cdef)

f:write('};')
f:write(string.format('std::size_t write_bytecode_size = %i;', #write_bytecode))

f:write([[
unsigned char read_bytecode[] = {
]])

f:write(read_cdef)

f:write('};')
f:write(string.format('std::size_t read_bytecode_size = %i;', #read_bytecode))

f:write([[
unsigned char write_at_bytecode[] = {
]])

f:write(write_at_cdef)

f:write('};')
f:write(string.format('std::size_t write_at_bytecode_size = %i;',
                      #write_at_bytecode))

f:write([[
unsigned char read_at_bytecode[] = {
]])

f:write(read_at_cdef)

f:write('};')
f:write(string.format('std::size_t read_at_bytecode_size = %i;',
                      #read_at_bytecode))

f:write('} // namespace emilua')
