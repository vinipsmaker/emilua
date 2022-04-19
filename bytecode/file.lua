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

function write_all_at_bootstrap(type, byte_span_append)
    return function(io_obj, offset, buffer)
       local ret = #buffer
       if type(buffer) == 'string' then
           buffer = byte_span_append(buffer)
       end
       while #buffer > 0 do
           local nwritten = io_obj:write_some_at(offset, buffer)
           offset = offset + nwritten
           buffer = buffer:slice(1 + nwritten)
       end
       return ret
    end
end

write_all_at_bytecode = string.dump(write_all_at_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(write_all_at_bytecode)
f:close()
write_all_at_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function read_all_at_bootstrap(io_obj, offset, buffer)
    local ret = #buffer
    while #buffer > 0 do
        local nread = io_obj:read_some_at(offset, buffer)
        offset = offset + nread
        buffer = buffer:slice(1 + nread)
    end
    return ret
end

read_all_at_bytecode = string.dump(read_all_at_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(read_all_at_bytecode)
f:close()
read_all_at_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char write_all_at_bytecode[] = {
]])

f:write(write_all_at_cdef)

f:write('};')
f:write(string.format('std::size_t write_all_at_bytecode_size = %i;',
                      #write_all_at_bytecode))

f:write([[
unsigned char read_all_at_bytecode[] = {
]])

f:write(read_all_at_cdef)

f:write('};')
f:write(string.format('std::size_t read_all_at_bytecode_size = %i;',
                      #read_all_at_bytecode))

f:write('} // namespace emilua')
