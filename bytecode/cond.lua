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

function cond_wait_bootstrap(error, native)
    return function(cnd, mtx)
        local e = native(cnd, mtx)
        mtx:lock()
        if e then
            error(e)
        end
    end
end

cond_wait_bytecode = string.dump(cond_wait_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(cond_wait_bytecode)
f:close()
cond_wait_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char cond_wait_bytecode[] = {
]])

f:write(cond_wait_cdef)

f:write('};')
f:write(string.format('std::size_t cond_wait_bytecode_size = %i;',
                      #cond_wait_bytecode))

f:write('} // namespace emilua')
