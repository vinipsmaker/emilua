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

function spawn_start_fn_bootstrap(root_scope, set_current_traceback,
                                  terminate_vm_with_cleanup_error, xpcall,
                                  pcall, error, unpack)
    return function(start_fn)
        return function()
            local ret = {xpcall(
                start_fn,
                function(e)
                    set_current_traceback()
                    return e
                end
            )}
            do
                local cleanup_handlers = root_scope()
                local i = #cleanup_handlers
                while i > 0 do
                    local ok = pcall(cleanup_handlers[i])
                    i = i - 1
                    if not ok then
                        terminate_vm_with_cleanup_error()
                    end
                end
            end
            if ret[1] == false then
                error(ret[2])
            end
            return unpack(ret, 2)
        end
    end
end

spawn_start_fn_bytecode = string.dump(spawn_start_fn_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(spawn_start_fn_bytecode)
f:close()
spawn_start_fn_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

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
unsigned char spawn_start_fn_bytecode[] = {
]])

f:write(spawn_start_fn_cdef)

f:write('};')
f:write(string.format('std::size_t spawn_start_fn_bytecode_size = %i;',
                      #spawn_start_fn_bytecode))

f:write([[
unsigned char fiber_join_bytecode[] = {
]])

f:write(fiber_join_cdef)

f:write('};')
f:write(string.format('std::size_t fiber_join_bytecode_size = %i;',
                      #fiber_join_bytecode))

f:write('} // namespace emilua')
