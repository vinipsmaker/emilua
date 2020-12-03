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
            error(e)
        end
    end
end

connect_bytecode = string.dump(connect_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(connect_bytecode)
f:close()

connect_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

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
f:write('} // namespace emilua')
