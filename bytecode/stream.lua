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

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
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

f:write('} // namespace emilua')
