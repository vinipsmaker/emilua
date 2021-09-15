OUTPUT = arg[#arg]

local function strip_xxd_hdr(file)
    local lines = {}
    for line in file:lines() do
        lines[#lines + 1] = line
    end
    table.remove(lines, 1)
    lines[#lines] = nil
    lines[#lines] = nil
    return table.concat(lines)
end

local function connect_bootstrap(error, native)
    return function(...)
        local e = native(...)
        if e then
            error(e, 0)
        end
    end
end

connect_bytecode = string.dump(connect_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(connect_bytecode)
f:close()
connect_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

local function data_op_bootstrap(error, native)
    return function(...)
        local e, bytes_transferred = native(...)
        if e then
            error(e, 0)
        end
        return bytes_transferred
    end
end

data_op_bytecode = string.dump(data_op_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(data_op_bytecode)
f:close()
data_op_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

local function accept_bootstrap(error, native)
    return function(...)
        local e, v = native(...)
        if e then
            error(e, 0)
        end
        return v
    end
end

accept_bytecode = string.dump(accept_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(accept_bytecode)
f:close()
accept_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

local function resolve_bootstrap(error, native)
    return function(...)
        local e, v = native(...)
        if e then
            error(e, 0)
        end
        return v
    end
end

resolve_bytecode = string.dump(resolve_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(resolve_bytecode)
f:close()
resolve_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char connect_bytecode[] = {
]])

f:write(connect_cdef)

f:write('};')
f:write(string.format('std::size_t connect_bytecode_size = %i;',
                      #connect_bytecode))

f:write([[
unsigned char data_op_bytecode[] = {
]])

f:write(data_op_cdef)

f:write('};')
f:write(string.format('std::size_t data_op_bytecode_size = %i;',
                      #data_op_bytecode))

f:write([[
unsigned char accept_bytecode[] = {
]])

f:write(accept_cdef)

f:write('};')
f:write(string.format('std::size_t accept_bytecode_size = %i;',
                      #accept_bytecode))

f:write([[
unsigned char resolve_bytecode[] = {
]])

f:write(resolve_cdef)

f:write('};')
f:write(string.format('std::size_t resolve_bytecode_size = %i;',
                      #resolve_bytecode))

f:write('} // namespace emilua')
