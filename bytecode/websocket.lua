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

function websocket_op_bootstrap(error, native)
    return function(...)
        local e, r1, r2 = native(...)
        if e then
            error(e, 0)
        end
        return r1, r2
    end
end

websocket_op_bytecode = string.dump(websocket_op_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(websocket_op_bytecode)
f:close()
websocket_op_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char websocket_op_bytecode[] = {
]])

f:write(websocket_op_cdef)

f:write('};')
f:write(string.format('std::size_t websocket_op_bytecode_size = %i;',
        #websocket_op_bytecode))

f:write('} // namespace emilua')
