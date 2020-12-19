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

function http_op_bootstrap(error, native)
    return function(self, arg1)
        local e = native(self, arg1)
        if e then
            error(e)
        end
    end
end

http_op_bytecode = string.dump(http_op_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(http_op_bytecode)
f:close()
http_op_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char http_op_bytecode[] = {
]])

f:write(http_op_cdef)

f:write('};')
f:write(string.format('std::size_t http_op_bytecode_size = %i;',
                      #http_op_bytecode))

f:write('} // namespace emilua')
