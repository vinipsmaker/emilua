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

function scope_bootstrap(scope_push, scope_pop, terminate_vm_with_cleanup_error,
                         restore_interruption, old_pcall, error)
    return function(f)
        scope_push()
        local ok, e = old_pcall(f)
        do
            local cleanup_handlers = scope_pop()
            local i = #cleanup_handlers
            while i > 0 do
                local ok = old_pcall(cleanup_handlers[i])
                i = i - 1
                if not ok then
                    terminate_vm_with_cleanup_error()
                end
            end
            -- scope_pop() already calls disable_interruption()
            restore_interruption()
        end
        if not ok then
            error(e, 0)
        end
    end
end

scope_bytecode = string.dump(scope_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scope_bytecode)
f:close()
scope_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function scope_cleanup_pop_bootstrap(scope_cleanup_pop, restore_interruption,
                                     terminate_vm_with_cleanup_error, pcall)
    return function(execute)
        if execute == nil then
            execute = true
        end
        local cleanup_handler = scope_cleanup_pop()
        if not execute then
            -- scope_cleanup_pop() already calls disable_interruption()
            restore_interruption()
            return
        end

        local ok = pcall(cleanup_handler)
        if not ok then
            terminate_vm_with_cleanup_error()
        end
        restore_interruption()
    end
end

scope_cleanup_pop_bytecode = string.dump(scope_cleanup_pop_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scope_cleanup_pop_bytecode)
f:close()
scope_cleanup_pop_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char scope_bytecode[] = {
]])

f:write(scope_cdef)

f:write('};')
f:write(string.format('std::size_t scope_bytecode_size = %i;', #scope_bytecode))

f:write('unsigned char scope_cleanup_pop_bytecode[] = {')

f:write(scope_cleanup_pop_cdef)

f:write('};')
f:write(string.format('std::size_t scope_cleanup_pop_bytecode_size = %i;',
                      #scope_cleanup_pop_bytecode))

f:write('} // namespace emilua')
