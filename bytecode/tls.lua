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

local function handshake_bootstrap(error, native)
    return function(...)
        local e = native(...)
        if e then
            error(e, 0)
        end
    end
end

handshake_bytecode = string.dump(handshake_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(handshake_bytecode)
f:close()
handshake_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char handshake_bytecode[] = {
]])

f:write(handshake_cdef)

f:write('};')
f:write(string.format('std::size_t handshake_bytecode_size = %i;',
                      #handshake_bytecode))

f:write('} // namespace emilua')
