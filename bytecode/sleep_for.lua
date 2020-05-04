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

local function sleep_for_bootstrap(error, native)
    return function(...)
        local e, v = native(...)
        if e then
            error(e)
        else
            return v
        end
    end
end

sleep_for_bytecode = string.dump(sleep_for_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(sleep_for_bytecode)
f:close()

sleep_for_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char sleep_for_bytecode[] = {
]])

f:write(sleep_for_cdef)

f:write('};')
f:write(string.format('std::size_t sleep_for_bytecode_size = %i;',
                      #sleep_for_bytecode))
f:write('} // namespace emilua')
