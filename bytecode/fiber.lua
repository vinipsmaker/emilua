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

function fiber_join_bootstrap(error, unpack, native)
    return function(...)
        local args = {native(...)}
        if args[1] then
            return unpack(args, 2)
        else
            error(args[2])
        end
    end
end

fiber_join_bytecode = string.dump(fiber_join_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(fiber_join_bytecode)
f:close()
fiber_join_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char fiber_join_bytecode[] = {
]])

f:write(fiber_join_cdef)

f:write('};')
f:write(string.format('std::size_t fiber_join_bytecode_size = %i;',
                      #fiber_join_bytecode))

f:write('} // namespace emilua')
