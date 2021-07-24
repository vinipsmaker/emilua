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

local function signal_set_wait_bootstrap(error, native)
    return function(...)
        local e, v = native(...)
        if e then
            error(e, 0)
        end
        return v
    end
end

signal_set_wait_bytecode = string.dump(signal_set_wait_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(signal_set_wait_bytecode)
f:close()
signal_set_wait_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char signal_set_wait_bytecode[] = {
]])

f:write(signal_set_wait_cdef)

f:write('};')
f:write(string.format('std::size_t signal_set_wait_bytecode_size = %i;',
                      #signal_set_wait_bytecode))

f:write('} // namespace emilua')
