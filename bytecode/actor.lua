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

function chan_op_bootstrap(error, native, type)
    return function(chan, ...)
        local e, r = native(chan, ...)
        if e then
            error(e)
        end
        if type(r) == 'function' then
            return r()
        end
        return r
    end
end

chan_op_bytecode = string.dump(chan_op_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(chan_op_bytecode)
f:close()
chan_op_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char chan_op_bytecode[] = {
]])

f:write(chan_op_cdef)

f:write('};')
f:write(string.format('std::size_t chan_op_bytecode_size = %i;',
                      #chan_op_bytecode))

f:write('} // namespace emilua')
